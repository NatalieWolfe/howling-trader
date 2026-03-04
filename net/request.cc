#include "net/request.h"

#include <string>

#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/version.hpp"
#include "net/connect.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::net {

namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;

http::response<http::string_body> post(
    connection& conn,
    std::string_view host,
    std::string_view target,
    const Json::Value& body) {
  http::request<http::string_body> req{http::verb::post, target, 11};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.set(http::field::content_type, "application/json");
  req.body() = howling::to_string(body);
  req.prepare_payload();

  http::write(conn.stream(), req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(conn.stream(), buffer, res);

  return res;
}

} // namespace howling::net
