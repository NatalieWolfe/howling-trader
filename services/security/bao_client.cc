#include "services/security/bao_client.h"

#include <stdexcept>
#include <string_view>

#include "absl/flags/flag.h"
#include "boost/beast/core/detail/config.hpp"
#include "boost/beast/http/basic_dynamic_body_fwd.hpp"
#include "boost/url/detail/config.hpp"
#include "json/value.h"

ABSL_FLAG(
    std::string,
    bao_address,
    "http://127.0.0.1:8200",
    "OpenBao server address.");

namespace howling::security {

namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;
namespace urls = ::boost::urls;

bao_client::bao_client() {}

void bao_client::wait_for_ready(std::chrono::milliseconds timeout) {
  throw std::runtime_error("Not implemented yet.");
}

Json::Value bao_client::get_secret(std::string_view path) {
  throw std::runtime_error("Not implemented yet.");
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
