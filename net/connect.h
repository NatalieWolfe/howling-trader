#pragma once

#include <memory>

#include "boost/asio/io_context.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/websocket/ssl.hpp"
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

class insecure_connection {
public:
  using stream_type = ::boost::beast::tcp_stream;

  stream_type& stream() { return _stream; }

private:
  friend std::unique_ptr<insecure_connection>
  make_insecure_connection(const url&);

  insecure_connection();

  boost::asio::io_context _io_context;
  stream_type _stream;
};

class websocket {
public:
  using stream_type = ::boost::beast::websocket::stream<
      ::howling::net::connection::stream_type>;

  stream_type& stream() { return _stream; }

private:
  friend std::unique_ptr<websocket> make_websocket(const url&);

  websocket();

  boost::asio::io_context _io_context;
  boost::asio::ssl::context _ssl_context;
  stream_type _stream;
};

std::unique_ptr<connection> make_connection(const url& u);
std::unique_ptr<insecure_connection> make_insecure_connection(const url& u);
std::unique_ptr<websocket> make_websocket(const url& u);

} // namespace howling::net
