#pragma once

#include <string>

#include "boost/beast/http/dynamic_body.hpp"
#include "boost/beast/http/message.hpp"

namespace howling::schwab {

/**
 * @brief Extracts the HTTP response body and decompresses it if it is encoded
 * with gzip or deflate.
 *
 * @param res The HTTP response to extract the body from.
 *
 * @throws std::runtime_error
 *    If decompression fails on corrupted or invalid data.
 *
 * @return The decompressed response body as a string.
 */
std::string get_response_body(
    const boost::beast::http::response<boost::beast::http::dynamic_body>& res);

} // namespace howling::schwab
