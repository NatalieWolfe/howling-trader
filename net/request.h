#pragma once

#include <functional>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "boost/beast/http/message_fwd.hpp"
#include "boost/beast/http/string_body_fwd.hpp"
#include "net/connect.h"
#include "json/value.h"

namespace howling::net {

// The kinds of connections which are supported in HTTP requests.
using variant_connection = std::variant<
    std::reference_wrapper<connection>,
    std::reference_wrapper<insecure_connection>>;

using headers_map = std::unordered_map<std::string, std::string>;

struct post_request {
  variant_connection conn;
  std::string_view host;
  std::string_view target;
  const headers_map& headers = {};
  const Json::Value& body = {};
};

/**
 * @brief Sends an HTTP POST request with a JSON body.
 *
 * @param conn The connection to use.
 * @param host The value for the Host header.
 * @param target The request target (path).
 * @param body The JSON body to send.
 *
 * @return The HTTP response.
 *
 * @throws std::runtime_error on failure.
 */
boost::beast::http::response<boost::beast::http::string_body>
post(const post_request& req);

struct get_request {
  variant_connection conn;
  std::string_view host;
  std::string_view target;
  const headers_map& headers = {};
};

/**
 * @brief Sends an HTTP GET request.
 *
 * @param conn The connection to use.
 * @param host The value for the Host header.
 * @param target The request target (path).
 *
 * @return The HTTP response.
 *
 * @throws std::runtime_error on failure.
 */
boost::beast::http::response<boost::beast::http::string_body>
get(const get_request& req);

} // namespace howling::net
