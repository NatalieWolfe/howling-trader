#include "net/request.h"

#include <string>
#include <utility>

#include "boost/asio/buffer.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/impl/buffers_cat.hpp"
#include "boost/beast/core/impl/buffers_prefix.hpp"
#include "boost/beast/core/impl/buffers_suffix.hpp"
#include "boost/beast/core/impl/flat_buffer.hpp"
#include "boost/beast/core/string_type.hpp"
#include "boost/beast/http/detail/rfc7230.hpp"
#include "boost/beast/http/empty_body.hpp"
#include "boost/beast/http/error.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/fields.hpp"
#include "boost/beast/http/fields_fwd.hpp"
#include "boost/beast/http/impl/basic_parser.hpp"
#include "boost/beast/http/impl/error.hpp"
#include "boost/beast/http/impl/fields.hpp"
#include "boost/beast/http/impl/message.hpp"
#include "boost/beast/http/impl/read.hpp"
#include "boost/beast/http/impl/rfc7230.hpp"
#include "boost/beast/http/impl/serializer.hpp"
#include "boost/beast/http/impl/write.hpp"
#include "boost/beast/http/serializer.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/http/verb.hpp"
#include "boost/beast/version.hpp"
#include "boost/intrusive/detail/algo_type.hpp"
#include "boost/intrusive/detail/list_iterator.hpp"
#include "boost/intrusive/detail/tree_iterator.hpp"
#include "boost/intrusive/link_mode.hpp"
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

http::response<http::string_body>
get(connection& conn, std::string_view host, std::string_view target) {
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
