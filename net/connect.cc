#include "net/connect.h"

#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "net/url.h"

namespace howling::net {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;

} // namespace

connection::connection()
    : _io_context{}, _ssl_context{asio::ssl::context::tls_client},
      _stream{_io_context, _ssl_context} {}

websocket::websocket()
    : _io_context{}, _ssl_context{asio::ssl::context::tls_client},
      _stream{_io_context, _ssl_context} {}

std::unique_ptr<connection> make_connection(const url& u) {
  std::unique_ptr<connection> conn{new connection()};
  conn->_ssl_context.set_default_verify_paths();
  conn->_ssl_context.set_verify_mode(asio::ssl::verify_peer);

  asio::ip::tcp::resolver tcp_resolver(conn->_io_context);
  asio::ip::tcp::resolver::results_type host_resolution =
      tcp_resolver.resolve(u.host, u.service);

  if (!::SSL_set_tlsext_host_name(
          conn->_stream.native_handle(), u.host.c_str())) {
    throw beast::system_error{beast::error_code{
        static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()}};
  }

  beast::get_lowest_layer(conn->_stream).connect(host_resolution);
  conn->_stream.handshake(asio::ssl::stream_base::client);

  return conn;
}

std::unique_ptr<websocket> make_websocket(const url& u) {
  std::unique_ptr<websocket> conn{new websocket()};
  conn->_ssl_context.set_default_verify_paths();
  conn->_ssl_context.set_verify_mode(asio::ssl::verify_peer);

  asio::ip::tcp::resolver tcp_resolver(conn->_io_context);
  asio::ip::tcp::resolver::results_type host_resolution =
      tcp_resolver.resolve(u.host, u.service);

  if (!::SSL_set_tlsext_host_name(
          conn->_stream.next_layer().native_handle(), u.host.c_str())) {
    throw beast::system_error{beast::error_code{
        static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()}};
  }

  beast::get_lowest_layer(conn->_stream).connect(host_resolution);
  conn->_stream.next_layer().handshake(asio::ssl::stream_base::client);
  conn->_stream.handshake(u.host, u.target);

  return conn;
}

} // namespace howling::net
