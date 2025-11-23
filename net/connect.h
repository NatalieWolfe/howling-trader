#pragma once

#include <memory>

#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "net/url.h"

namespace howling::net {

class connection {
public:
  using stream_type = ::boost::asio::ssl::stream<boost::beast::tcp_stream>;

  stream_type& stream() { return _stream; }

private:
  friend std::unique_ptr<connection> make_connection(const url&);

  connection();

  boost::asio::io_context _io_context;
  boost::asio::ssl::context _ssl_context;
  stream_type _stream;
};

std::unique_ptr<connection> make_connection(const url& u);

} // namespace howling::net
