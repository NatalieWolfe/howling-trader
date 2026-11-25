#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/websocket/ssl.hpp"
#include "net/url.h"
#include "signaling/cancellation.h"

namespace howling::net {

/**
 * @brief Owns a TLS-encrypted TCP connection.
 *
 * Thread-compatible: callers must synchronize access across threads.
 */
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

/**
 * @brief Owns an unencrypted TCP connection.
 *
 * Thread-compatible: callers must synchronize access across threads.
 */
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

/**
 * @brief Configuration options for a TLS WebSocket connection.
 */
struct websocket_options {
  std::chrono::milliseconds connect_timeout = std::chrono::seconds{30};
  std::chrono::milliseconds read_timeout = std::chrono::seconds{35};
  std::chrono::milliseconds write_timeout = std::chrono::seconds{15};
  bool enable_keepalive = true;
  int keepalive_idle_seconds = 15;
  int keepalive_interval_seconds = 5;
  int keepalive_count = 3;
  // Optional PEM-encoded CA certificate for TLS verification. Uses system
  // default verification paths when empty.
  std::string ca_cert_pem;
  // Optional parent cancellation handle. When cancelled, any in-progress
  // WebSocket operation is aborted immediately.
  std::optional<std::reference_wrapper<const cancellation>>
      parent_cancellation = std::nullopt;
};

/**
 * @brief Owns a TLS-encrypted WebSocket connection with keepalive and timeouts.
 *
 * Thread-compatible for connect(), read(), write(), close(), and stream(),
 * which must be invoked by a single owner thread at a time. The handle returned
 * by cancellation() is thread-safe and may be used concurrently from any thread
 * to abort an in-progress operation or attach child cancellation handles.
 */
class websocket {
public:
  using stream_type = ::boost::beast::websocket::stream<
      ::howling::net::connection::stream_type>;

  explicit websocket(websocket_options options = {});
  websocket(const websocket&) = delete;
  websocket& operator=(const websocket&) = delete;

  stream_type& stream() { return _stream; }

  /**
   * @brief Resolves, connects, configures keepalive, and completes TLS and
   * WebSocket handshakes for the target URL.
   */
  void connect(const url& target_url);

  /**
   * @brief Reads a single WebSocket message with the configured read timeout.
   *
   * @throws boost::beast::system_error on timeout, cancellation, or I/O error.
   */
  std::string read();

  /**
   * @brief Writes a single WebSocket message with the configured write timeout.
   *
   * @throws boost::beast::system_error on timeout, cancellation, or I/O error.
   */
  void write(std::string_view message);

  /**
   * @brief Performs a normal WebSocket close handshake if not already
   * cancelled.
   */
  void close();

  /**
   * @brief Returns the cancellation handle for this WebSocket.
   */
  [[nodiscard]] howling::cancellation& cancellation() noexcept {
    return _cancellation;
  }

  [[nodiscard]] const howling::cancellation& cancellation() const noexcept {
    return _cancellation;
  }

private:
  void _configure_tls(const url& target_url);
  boost::asio::ip::tcp::resolver::results_type
  _resolve_host(const url& target_url);
  void
  _connect_tcp(const boost::asio::ip::tcp::resolver::results_type& endpoints);
  void _handshake_tls();
  void _handshake_websocket(const url& target_url);
  void _close_socket();

  websocket_options _options;
  howling::cancellation _cancellation;
  boost::asio::io_context _io_context;
  boost::asio::ip::tcp::resolver _resolver;
  boost::asio::ssl::context _ssl_context;
  stream_type _stream;
};

/**
 * @brief Opens a TLS-encrypted TCP connection to the specified URL.
 */
std::unique_ptr<connection> make_connection(const url& u);

/**
 * @brief Opens an unencrypted TCP connection to the specified URL.
 */
std::unique_ptr<insecure_connection> make_insecure_connection(const url& u);

/**
 * @brief Opens a TLS-encrypted WebSocket connection with keepalive and
 * timeouts.
 */
std::unique_ptr<websocket>
make_websocket(const url& u, const websocket_options& options = {});

} // namespace howling::net
