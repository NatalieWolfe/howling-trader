#include "services/security/bao_client.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "absl/flags/flag.h"
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

http::response<http::string_body> get_bao(std::string_view target) {
  urls::url url = urls::parse_uri(absl::GetFlag(FLAGS_bao_address)).value();
  net::url bao_host_url;
  bao_host_url.host = url.host();
  bao_host_url.service = url.port();
  if (bao_host_url.service.empty()) {
    bao_host_url.service = url.scheme() == "https" ? "443" : "80";
  }

  auto conn = net::make_insecure_connection(bao_host_url);
  return net::get(*conn, bao_host_url.host, target);
}

} // namespace

bao_client::bao_client() {}

void bao_client::wait_for_ready(milliseconds timeout) {
  const steady_clock::time_point start_time = steady_clock::now();
  milliseconds delay{10};

  while (true) {
    try {
      if (get_bao("/v1/sys/health").result() == http::status::ok) return;
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

  if (res.result() != http::status::ok) {
    throw std::runtime_error(
        absl::StrCat(
            "Failed to retrieve secret from OpenBao: ",
            static_cast<int>(res.result()),
            " ",
            res.body()));
  }

  Json::Value response = to_json(res.body());
  return response["data"]["data"];
}

std::string
bao_client::encrypt(std::string_view key_name, std::string_view plaintext) {
  throw std::runtime_error("Not implemented yet.");
}

std::string
bao_client::decrypt(std::string_view key_name, std::string_view ciphertext) {
  throw std::runtime_error("Not implemented yet.");
}

} // namespace howling::security
