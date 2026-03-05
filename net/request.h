#pragma once

#include <string_view>

#include "boost/beast/http/message_fwd.hpp"
#include "boost/beast/http/string_body_fwd.hpp"
#include "net/connect.h"
#include "json/value.h"

namespace howling::net {

/**
 * @brief Sends an HTTP POST request with a JSON body.
 *
 * @param conn The connection to use.
 * @param host The value for the Host header.
 * @param target The request target (path).
 * @param body The JSON body to send.
 * @return The HTTP response.
 *
 * @throws std::runtime_error on failure.
 */
boost::beast::http::response<boost::beast::http::string_body> post(
    connection& conn,
    std::string_view host,
    std::string_view target,
    const Json::Value& body);

/**
 * @brief Sends an HTTP GET request.
 *
 * @param conn The connection to use.
 * @param host The value for the Host header.
 * @param target The request target (path).
 * @return The HTTP response.
 *
 * @throws std::runtime_error on failure.
 */
boost::beast::http::response<boost::beast::http::string_body>
get(insecure_connection& conn, std::string_view host, std::string_view target);

} // namespace howling::net
