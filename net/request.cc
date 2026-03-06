#include "net/request.h"

#include <string>
#include <utility>

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
http::response<http::string_body> do_post(
    Stream& stream,
    std::string_view host,
    std::string_view target,
    const Json::Value& body) {
  http::request<http::string_body> req{http::verb::post, target, 11};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.set(http::field::content_type, "application/json");
  req.body() = howling::to_string(body);
  req.prepare_payload();

  http::write(stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res);

  return res;
}

} // namespace

http::response<http::string_body> post(
    connection& conn,
    std::string_view host,
    std::string_view target,
    const Json::Value& body) {
  return do_post(conn.stream(), host, target, body);
}

http::response<http::string_body> post(
    insecure_connection& conn,
    std::string_view host,
    std::string_view target,
    const Json::Value& body) {
  return do_post(conn.stream(), host, target, body);
}

http::response<http::string_body>
get(insecure_connection& conn, std::string_view host, std::string_view target) {
  http::request<http::empty_body> req{http::verb::get, target, 11};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  http::write(conn.stream(), req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(conn.stream(), buffer, res);

  return res;
}

} // namespace howling::net
