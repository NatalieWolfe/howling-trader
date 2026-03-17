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
#include "strings/json.h"
#include "json/value.h"

ABSL_FLAG(
    std::string,
    bao_address,
    "http://127.0.0.1:8200",
    "OpenBao server address.");

namespace howling::security {
namespace {

namespace http = ::boost::beast::http;
namespace urls = ::boost::urls;

using ::std::chrono::milliseconds;
using ::std::chrono::steady_clock;

constexpr milliseconds MAX_DELAY{2000};

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
  // TODO: Check the url scheme and use a secure connection if it is https.
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

void bao_client::wait_for_ready(milliseconds timeout) {
  const steady_clock::time_point start_time = steady_clock::now();
  milliseconds delay{1};

  while (true) {
    try {
      auto res = get_bao("/v1/sys/health");
      if (res.result() == http::status::ok) return;
      LOG(INFO) << "Failed to get bao status: " << res.result_int();
    } catch (...) {
      // Ignore connection errors and retry.
    }

    if (steady_clock::now() - start_time >= timeout) {
      throw std::runtime_error(
          "Timed out waiting for OpenBao proxy to be ready.");
    }

    std::this_thread::sleep_for(delay);
    delay = std::min(delay * 2, MAX_DELAY);
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

} // namespace howling::security
