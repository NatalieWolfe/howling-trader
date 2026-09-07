#include "net/request.h"

#include <string>
#include <utility>
#include <variant>

#include "boost/asio/buffer.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/http/empty_body.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/read.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/http/verb.hpp"
#include "boost/beast/http/write.hpp"
#include "boost/beast/version.hpp"
#include "net/connect.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::net {
namespace {

namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;

template <typename Stream>
http::response<http::string_body>
do_post(Stream& stream, const post_request& request) {
  http::request<http::string_body> req{http::verb::post, request.target, 11};
  req.set(http::field::host, request.host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.set(http::field::content_type, "application/json");
  for (const auto& [key, value] : request.headers) req.set(key, value);
  req.body() = howling::to_string(request.body);
  req.prepare_payload();
  http::write(stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res);

  return res;
}

template <typename Stream>
http::response<http::string_body>
do_get(Stream& stream, const get_request& request) {
  http::request<http::empty_body> req{http::verb::get, request.target, 11};
  req.set(http::field::host, request.host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  for (const auto& [key, value] : request.headers) req.set(key, value);
  http::write(stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res);

  return res;
}

} // namespace

http::response<http::string_body> post(const post_request& req) {
  return std::visit(
      [&](auto& conn) { return do_post(conn.get().stream(), req); }, req.conn);
}

http::response<http::string_body> get(const get_request& request) {
  return std::visit(
      [&](auto& conn) { return do_get(conn.get().stream(), request); },
      request.conn);
}

} // namespace howling::net
