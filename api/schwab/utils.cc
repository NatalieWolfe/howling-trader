#include "api/schwab/utils.h"

#include <string>

#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/http/dynamic_body.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/message.hpp"
#include "net/gzip.h"

namespace howling::schwab {

std::string get_response_body(
    const boost::beast::http::response<boost::beast::http::dynamic_body>& res) {
  std::string body = boost::beast::buffers_to_string(res.body().data());
  if (net::is_compressed_encoding(
          res[boost::beast::http::field::content_encoding]) ||
      net::is_compressed_encoding(
          res[boost::beast::http::field::transfer_encoding])) {
    return net::gzip_decompress(body);
  }
  return body;
}

} // namespace howling::schwab
