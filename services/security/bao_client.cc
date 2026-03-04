#include "services/security/bao_client.h"

#include <stdexcept>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "boost/beast.hpp"
#include "boost/url.hpp"
#include "files/files.h"
#include "net/connect.h"
#include "net/request.h"
#include "net/url.h"
#include "strings/json.h"
#include "json/value.h"

ABSL_FLAG(
    std::string,
    bao_address,
    "http://openbao.security.svc:8200",
    "OpenBao server address.");

ABSL_FLAG(
    std::string,
    bao_auth_path,
    "auth/kubernetes/login",
    "OpenBao Kubernetes auth mount point.");

ABSL_FLAG(
    std::string,
    bao_token_file,
    "/var/run/secrets/kubernetes.io/serviceaccount/token",
    "Path to the Kubernetes ServiceAccount token.");

namespace howling::security {

namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;
namespace urls = ::boost::urls;

bao_client::bao_client() {}

void bao_client::login() {
  std::string jwt = files::read_file(absl::GetFlag(FLAGS_bao_token_file));

  std::string address = absl::GetFlag(FLAGS_bao_address);
  urls::url url = urls::parse_uri(address).value();

  net::url bao_host_url;
  bao_host_url.host = url.host();
  bao_host_url.service = url.port();
  if (bao_host_url.service.empty()) {
    bao_host_url.service = url.scheme() == "https" ? "443" : "80";
  }
  auto conn = net::make_connection(bao_host_url);

  Json::Value body;
  body["role"] = "howling-role";
  body["jwt"] = jwt;

  auto res = net::post(
      *conn,
      bao_host_url.host,
      absl::StrCat("/v1/", absl::GetFlag(FLAGS_bao_auth_path)),
      body);

  if (res.result() != http::status::ok) {
    throw std::runtime_error(
        absl::StrCat(
            "OpenBao login failed: ",
            static_cast<int>(res.result()),
            " ",
            res.body()));
  }

  Json::Value response = to_json(res.body());
  _token = response["auth"]["client_token"].asString();
  if (_token.empty()) {
    throw std::runtime_error("OpenBao login returned an empty token.");
  }
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
