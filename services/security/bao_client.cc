#include "services/security/bao_client.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "boost/beast/http.hpp"
#include "boost/url/parse.hpp"
#include "boost/url/url.hpp"
#include "net/connect.h"
#include "net/request.h"
#include "net/url.h"
#include "services/security/certificate_bundle.h"
#include "strings/json.h"
#include "json/value.h"

ABSL_FLAG(
    std::string,
    bao_address,
    "http://127.0.0.1:8200",
    "OpenBao server address.");

ABSL_FLAG(
    bool,
    bao_shutdown_on_destroy,
    false,
    "Instruct the Bao API proxy to shutdown when the client is destroyed.");

namespace howling::security {
namespace {

namespace http = ::boost::beast::http;
namespace urls = ::boost::urls;

using ::std::chrono::duration_cast;
using ::std::chrono::milliseconds;
using ::std::chrono::steady_clock;

constexpr double BACKOFF_EXP = 1.2;
constexpr steady_clock::duration MAX_DELAY = milliseconds{10000};

net::url make_bao_url() {
  urls::url url = urls::parse_uri(absl::GetFlag(FLAGS_bao_address)).value();
  net::url bao_url{.service = url.port(), .host = url.host()};
  if (bao_url.service.empty()) {
    bao_url.service = url.scheme() == "https" ? "443" : "80";
  }
  return bao_url;
}

http::response<http::string_body> get_bao(std::string_view target) {
  net::url bao_url = make_bao_url();
  // TODO: #106 - Check the url scheme and use a secure connection if it is
  // https.
  auto conn = net::make_insecure_connection(bao_url);
  return net::get(*conn, bao_url.host, target);
}

http::response<http::string_body>
post_bao(std::string_view target, const Json::Value& body) {
  net::url bao_url = make_bao_url();
  auto conn = net::make_insecure_connection(bao_url);
  return net::post(*conn, bao_url.host, target, body);
}

void check_response(
    const http::response<http::string_body>& res, std::string_view action) {
  if (res.result() != http::status::ok) {
    throw std::runtime_error(
        std::format(
            "Failed to {} via OpenBao: {} {}",
            action,
            static_cast<int>(res.result()),
            res.body()));
  }
}

} // namespace

bao_client::bao_client() {}

bao_client::~bao_client() {
  if (absl::GetFlag(FLAGS_bao_shutdown_on_destroy)) {
    try {
      post_bao("/agent/v1/quit", Json::objectValue);
    } catch (const std::exception& e) {
      LOG(WARNING) << "Error shutting down Bao sidecar: " << e.what();
    } catch (...) {
      LOG(WARNING) << "Unknown error shutting down Bao sidecar.";
    }
  }
}

void bao_client::wait_for_ready(milliseconds timeout) {
  const steady_clock::time_point start_time = steady_clock::now();
  steady_clock::duration delay = milliseconds{1};

  int connection_failure_count = 0;
  int status_failure_count = 0;
  steady_clock::time_point start = steady_clock::now();
  while (true) {
    try {
      auto res = get_bao("/v1/sys/health");
      if (status_failure_count == 0) {
        LOG(INFO)
            << "Established connection in "
            << duration_cast<milliseconds>(steady_clock::now() - start).count()
            << " milliseconds after " << connection_failure_count
            << " connection failures.";
      }
      if (res.result() == http::status::ok) {
        LOG(INFO)
            << "Successful Bao health check in "
            << duration_cast<milliseconds>(steady_clock::now() - start).count()
            << " milliseconds after " << connection_failure_count
            << " connection failures and " << status_failure_count
            << " status check failures.";
        return;
      }
      ++status_failure_count;
      LOG(INFO) << "Failed to get bao status: " << res.result_int();
    } catch (const std::exception& e) {
      // Ignore connection errors and retry.
      ++connection_failure_count;
    } catch (...) {
      LOG(INFO) << "Unknown connection error waiting for bao to start.";
      ++connection_failure_count;
    }

    if (steady_clock::now() - start_time >= timeout) {
      LOG(ERROR)
          << "Failed to connect to Bao after "
          << duration_cast<milliseconds>(steady_clock::now() - start).count()
          << " milliseconds with " << connection_failure_count
          << " connection failures and " << status_failure_count
          << " status check failures.";
      throw std::runtime_error(
          "Timed out waiting for OpenBao proxy to be ready.");
    }

    std::this_thread::sleep_for(delay);
    delay = std::min(
        duration_cast<steady_clock::duration>(delay * BACKOFF_EXP), MAX_DELAY);
  }
}

Json::Value bao_client::get_secret(std::string_view path) {
  auto res = get_bao(absl::StrCat("/v1/secret/data/", path));
  check_response(res, "retrieve secret");
  Json::Value response = to_json(res.body());
  return response["data"]["data"];
}

std::string
bao_client::encrypt(std::string_view key_name, std::string_view plaintext) {
  Json::Value body;
  body["plaintext"] = absl::Base64Escape(plaintext);
  auto res = post_bao(absl::StrCat("/v1/transit/encrypt/", key_name), body);
  check_response(res, "encrypt data");
  Json::Value response = to_json(res.body());
  return response["data"]["ciphertext"].asString();
}

std::string
bao_client::decrypt(std::string_view key_name, std::string_view ciphertext) {
  Json::Value body;
  body["ciphertext"] = std::string(ciphertext);
  auto res = post_bao(absl::StrCat("/v1/transit/decrypt/", key_name), body);
  check_response(res, "decrypt data");

  Json::Value response = to_json(res.body());
  std::string plaintext;
  if (!absl::Base64Unescape(
          response["data"]["plaintext"].asString(), &plaintext)) {
    throw std::runtime_error("Failed to base64 decode plaintext from OpenBao.");
  }
  return plaintext;
}

certificate_bundle bao_client::create_certificate(
    std::string_view role, std::string_view common_name) {
  Json::Value body;
  body["common_name"] = std::string{common_name};
  auto res = post_bao(absl::StrCat("/v1/pki/issue/", role), body);
  check_response(res, "create certificate");

  Json::Value response = to_json(res.body());
  const Json::Value* data_itr = response.find("data");
  if (!data_itr) {
    throw std::runtime_error("No `data` element in response from OpenBao!");
  }

  const Json::Value* cert_itr = data_itr->find("certificate");
  const Json::Value* private_key_itr = data_itr->find("private_key");
  const Json::Value* ca_chain_itr = data_itr->find("ca_chain");
  if (!cert_itr || !private_key_itr || !ca_chain_itr) {
    throw std::runtime_error(
        "Unexpected response body. Missing required field(s).");
  }

  certificate_bundle bundle{
      .certificate = cert_itr->asString(),
      .private_key = private_key_itr->asString(),
  };
  bundle.ca_chain = absl::StrCat(bundle.certificate, "\n");
  for (const Json::Value& ca : *ca_chain_itr) {
    absl::StrAppend(&bundle.ca_chain, ca.asString(), "\n");
  }

  return bundle;
}

std::string bao_client::get_ca_certificate() {
  auto res = get_bao("/v1/pki/ca/pem");
  check_response(res, "get root certificate");
  return res.body();
}

} // namespace howling::security
