#include "net/connect.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#ifdef __linux__
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

#include "absl/log/log.h"
#include "boost/asio/buffer.hpp"
#include "boost/asio/error.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/socket_base.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/beast.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/stream_traits.hpp"
#include "net/url.h"
#include "openssl/err.h"
#include "openssl/ssl.h"

namespace howling::net {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;

void configure_keepalive(
    asio::ip::tcp::socket& socket, const websocket_options& options) {
  if (!options.enable_keepalive) return;

  socket.set_option(asio::socket_base::keep_alive(true));
#ifdef __linux__
  int native_fd = socket.native_handle();
  if (options.keepalive_idle_seconds > 0) {
    int idle = options.keepalive_idle_seconds;
    if (::setsockopt(
            native_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) != 0) {
      LOG(WARNING) << "Failed to set TCP_KEEPIDLE: " << std::strerror(errno);
    }
  }
  if (options.keepalive_interval_seconds > 0) {
    int interval = options.keepalive_interval_seconds;
    if (::setsockopt(
            native_fd,
            IPPROTO_TCP,
            TCP_KEEPINTVL,
            &interval,
            sizeof(interval)) != 0) {
      LOG(WARNING) << "Failed to set TCP_KEEPINTVL: " << std::strerror(errno);
    }
  }
  if (options.keepalive_count > 0) {
    int count = options.keepalive_count;
    if (::setsockopt(
            native_fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0) {
      LOG(WARNING) << "Failed to set TCP_KEEPCNT: " << std::strerror(errno);
    }
  }
#endif
}

template <typename StartOp, typename CancelOp>
void run_with_timeout(
    asio::io_context& io_context,
    std::chrono::milliseconds timeout,
    const std::atomic_bool& cancelled,
    StartOp&& start_op,
    CancelOp&& cancel_op) {
  if (cancelled.load()) {
    throw beast::system_error{asio::error::operation_aborted};
  }

  beast::error_code op_error;
  bool timed_out = false;
  asio::steady_timer timer{io_context};

  io_context.restart();
  if (timeout > std::chrono::milliseconds{0}) {
    timer.expires_after(timeout);
    timer.async_wait([&timed_out, &cancel_op](const beast::error_code& ec) {
      if (!ec) {
        timed_out = true;
        cancel_op();
      }
    });
  }

  start_op([&op_error, &timer](const beast::error_code& ec) {
    op_error = ec;
    timer.cancel();
  });

  io_context.run();

  if (cancelled.load()) {
    throw beast::system_error{asio::error::operation_aborted};
  }
  if (timed_out) { throw beast::system_error{beast::error::timeout}; }
  if (op_error) { throw beast::system_error{op_error}; }
}

} // namespace

connection::connection()
    : _io_context{}, _ssl_context{asio::ssl::context::tls_client},
      _stream{_io_context, _ssl_context} {}

insecure_connection::insecure_connection()
    : _io_context{}, _stream{_io_context} {}

websocket::websocket(websocket_options options)
    : _options{std::move(options)}, _io_context{}, _resolver{_io_context},
      _ssl_context{asio::ssl::context::tls_client},
      _stream{_io_context, _ssl_context} {}

void websocket::_close_socket() {
  auto& lowest = beast::get_lowest_layer(_stream);
  lowest.cancel();
  if (lowest.socket().is_open()) {
    beast::error_code ec;
    lowest.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    lowest.close();
  }
}

void websocket::connect(const url& target_url) {
  if (_cancelled.load()) {
    throw beast::system_error{asio::error::operation_aborted};
  }

  if (_options.ca_cert_pem.empty()) {
    _ssl_context.set_default_verify_paths();
  } else {
    _ssl_context.add_certificate_authority(asio::buffer(_options.ca_cert_pem));
  }
  _ssl_context.set_verify_mode(asio::ssl::verify_peer);

  if (!::SSL_set_tlsext_host_name(
          _stream.next_layer().native_handle(), target_url.host.c_str())) {
    throw beast::system_error{beast::error_code{
        static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()}};
  }

  // Step 1: Resolve the remote host asynchronously with a connect timeout.
  asio::ip::tcp::resolver::results_type host_resolution;
  run_with_timeout(
      _io_context,
      _options.connect_timeout,
      _cancelled,
      [&](auto on_done) {
        _resolver.async_resolve(
            target_url.host,
            target_url.service,
            [&host_resolution, done = std::move(on_done)](
                const beast::error_code& ec,
                asio::ip::tcp::resolver::results_type results) mutable {
              host_resolution = std::move(results);
              done(ec);
            });
      },
      [this]() { _resolver.cancel(); });

  // Step 2: Establish the TCP connection with a connect timeout.
  auto& lowest = beast::get_lowest_layer(_stream);
  lowest.expires_never();
  run_with_timeout(
      _io_context,
      _options.connect_timeout,
      _cancelled,
      [&](auto on_done) {
        lowest.async_connect(
            host_resolution,
            [done = std::move(on_done)](
                const beast::error_code& ec,
                const asio::ip::tcp::resolver::results_type::
                    endpoint_type&) mutable { done(ec); });
      },
      [this]() { _close_socket(); });

  configure_keepalive(lowest.socket(), _options);

  // Step 3: Perform the TLS handshake with a connect timeout.
  run_with_timeout(
      _io_context,
      _options.connect_timeout,
      _cancelled,
      [&](auto on_done) {
        _stream.next_layer().async_handshake(
            asio::ssl::stream_base::client, std::move(on_done));
      },
      [this]() { _close_socket(); });

  // Step 4: Perform the WebSocket upgrade handshake with a connect timeout.
  run_with_timeout(
      _io_context,
      _options.connect_timeout,
      _cancelled,
      [&](auto on_done) {
        _stream.async_handshake(
            target_url.host, target_url.target, std::move(on_done));
      },
      [this]() { _close_socket(); });
}

std::string websocket::read() {
  beast::flat_buffer buffer;
  run_with_timeout(
      _io_context,
      _options.read_timeout,
      _cancelled,
      [&](auto on_done) {
        _stream.async_read(
            buffer,
            [done = std::move(on_done)](
                const beast::error_code& ec, std::size_t) mutable {
              done(ec);
            });
      },
      [this]() { _close_socket(); });
  return beast::buffers_to_string(buffer.cdata());
}

void websocket::write(std::string_view message) {
  run_with_timeout(
      _io_context,
      _options.write_timeout,
      _cancelled,
      [&](auto on_done) {
        _stream.async_write(
            asio::buffer(message),
            [done = std::move(on_done)](
                const beast::error_code& ec, std::size_t) mutable {
              done(ec);
            });
      },
      [this]() { _close_socket(); });
}

void websocket::close() {
  if (_cancelled.load()) return;

  try {
    run_with_timeout(
        _io_context,
        _options.write_timeout,
        _cancelled,
        [&](auto on_done) {
          _stream.async_close(
              beast::websocket::close_code::normal, std::move(on_done));
        },
        [this]() { _close_socket(); });
  } catch (const beast::system_error& err) {
    if (err.code() != asio::error::operation_aborted &&
        err.code() != asio::error::eof &&
        err.code() != asio::error::broken_pipe &&
        err.code() != asio::error::connection_reset &&
        err.code() != asio::ssl::error::stream_truncated &&
        err.code() != beast::websocket::error::closed) {
      throw;
    }
  }
}

void websocket::cancel() {
  if (_cancelled.exchange(true)) return;
  asio::post(_io_context, [this]() {
    _resolver.cancel();
    _close_socket();
  });
}

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

std::unique_ptr<insecure_connection> make_insecure_connection(const url& u) {
  std::unique_ptr<insecure_connection> conn{new insecure_connection()};

  asio::ip::tcp::resolver tcp_resolver(conn->_io_context);
  asio::ip::tcp::resolver::results_type host_resolution =
      tcp_resolver.resolve(u.host, u.service);

  conn->_stream.connect(host_resolution);

  return conn;
}

std::unique_ptr<websocket>
make_websocket(const url& u, const websocket_options& options) {
  auto conn = std::make_unique<websocket>(options);
  conn->connect(u);
  return conn;
}

} // namespace howling::net
