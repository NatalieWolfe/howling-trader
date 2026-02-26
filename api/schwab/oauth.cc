#include "api/schwab/oauth.h"

#include <format>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "api/schwab/configuration.h"
#include "api/schwab/connect.h"
#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/read.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/http/verb.hpp"
#include "boost/beast/http/write.hpp"
#include "boost/url/url.hpp"
#include "net/connect.h"
#include "strings/json.h"
#include "json/json.h"

namespace howling::schwab {
namespace {

namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;
namespace urls = ::boost::urls;

} // namespace

std::string make_schwab_authorize_url(
    std::string_view client_id, std::string_view redirect_url) {
  urls::url url;
  url.set_scheme("https");
  url.set_host(get_schwab_host());
  url.set_path("/v1/oauth/authorize");
  url.params().append({"client_id", client_id});
  url.params().append({"redirect_uri", redirect_url});
  url.params().append({"response_type", "code"});
  return url.c_str();
}

oauth_tokens
exchange_code_for_tokens(net::connection& conn, std::string_view code) {
  check_schwab_flags();

  net::url oauth_url = make_net_url("/v1/oauth/token");
  LOG(INFO) << "POST " << oauth_url.target;

  using http_headers = ::boost::beast::http::field;
  http::request<http::string_body> req{http::verb::post, oauth_url.target, 11};
  req.set(http_headers::host, oauth_url.host);
  req.set(
      http_headers::authorization,
      std::format(
          "Basic {}",
          absl::Base64Escape(
              absl::StrCat(
                  absl::GetFlag(FLAGS_schwab_api_key_id),
                  ":",
                  absl::GetFlag(FLAGS_schwab_api_key_secret)))));
  req.set(http_headers::accept, "application/json");

  urls::url body;
  body.params().append({"grant_type", "authorization_code"});
  body.params().append(
      {"redirect_uri", absl::GetFlag(FLAGS_schwab_oauth_redirect_url)});
  body.params().append({"code", code});

  req.set(http_headers::content_length, std::to_string(body.query().size()));
  req.set(http_headers::content_type, "application/x-www-form-urlencoded");
  req.body() = body.query();

  http::write(conn.stream(), req);
  beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  http::read(conn.stream(), buffer, res);

  if (res.result_int() != 200) {
    LOG(ERROR) << "Schwab API responded with " << res.result_int();
    throw std::runtime_error(
        std::format(
            "Bad response from Schwab API server: {} {}",
            res.result_int(),
            std::string_view{res.reason()}));
  }

  Json::Value root = to_json(beast::buffers_to_string(res.body().data()));
  return oauth_tokens{
      .access_token = root["access_token"].asString(),
      .refresh_token = root["refresh_token"].asString(),
      .expires_in = root["expires_in"].asInt()};
}

oauth_tokens
refresh_tokens(net::connection& conn, std::string_view refresh_token) {
  check_schwab_flags();

  net::url oauth_url = make_net_url("/v1/oauth/token");
  LOG(INFO) << "POST " << oauth_url.target;

  using http_headers = ::boost::beast::http::field;
  http::request<http::string_body> req{http::verb::post, oauth_url.target, 11};
  req.set(http_headers::host, oauth_url.host);
  req.set(
      http_headers::authorization,
      std::format(
          "Basic {}",
          absl::Base64Escape(
              absl::StrCat(
                  absl::GetFlag(FLAGS_schwab_api_key_id),
                  ":",
                  absl::GetFlag(FLAGS_schwab_api_key_secret)))));
  req.set(http_headers::accept, "application/json");

  urls::url body;
  body.params().append({"grant_type", "refresh_token"});
  body.params().append({"refresh_token", refresh_token});

  req.set(http_headers::content_length, std::to_string(body.query().size()));
  req.set(http_headers::content_type, "application/x-www-form-urlencoded");
  req.body() = body.query();

  http::write(conn.stream(), req);
  beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  http::read(conn.stream(), buffer, res);

  if (res.result_int() != 200) {
    LOG(ERROR) << beast::buffers_to_string(res.body().data());
    throw std::runtime_error(
        std::format(
            "Bad response from Schwab API server: {} {}",
            res.result_int(),
            std::string_view{res.reason()}));
  }

  Json::Value root = to_json(beast::buffers_to_string(res.body().data()));
  return oauth_tokens{
      .access_token = root["access_token"].asString(),
      .refresh_token =
          root.get("refresh_token", std::string{refresh_token}).asString(),
      .expires_in = root["expires_in"].asInt()};
}

} // namespace howling::schwab
