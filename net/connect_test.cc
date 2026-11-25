#include "net/connect.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <utility>

#include "boost/asio/buffer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/socket_base.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/stream_traits.hpp"
#include "boost/beast/websocket.hpp"
#include "boost/beast/websocket/ssl.hpp"
#include "net/test_tls_cert.h"
#include "net/url.h"
#include "signaling/cancellation.h"
#include "gtest/gtest.h"

namespace howling::net {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
using namespace ::std::chrono_literals;

using server_ws =
    beast::websocket::stream<asio::ssl::stream<asio::ip::tcp::socket>>;

class loopback_tls_server {
public:
  using session_handler = std::function<void(loopback_tls_server&, server_ws&)>;

  loopback_tls_server(
      const testing::self_signed_cert& cert, session_handler handler)
      : _ssl_ctx{asio::ssl::context::tls_server},
        _acceptor{
            _io_context,
            asio::ip::tcp::endpoint{asio::ip::make_address("127.0.0.1"), 0}},
        _handler{std::move(handler)} {
    _ssl_ctx.use_certificate_chain(asio::buffer(cert.cert_pem));
    _ssl_ctx.use_private_key(
        asio::buffer(cert.key_pem), asio::ssl::context::pem);
    _port = _acceptor.local_endpoint().port();
    _thread = std::jthread([this]() { _serve(); });
  }

  ~loopback_tls_server() {
    _stopping = true;
    beast::error_code ec;
    _acceptor.close(ec);
    _notify_release();
  }

  [[nodiscard]] uint16_t port() const { return _port; }

  void release_session() { _notify_release(); }

  void wait_until_released() {
    std::unique_lock lock{_release_mutex};
    _release_cv.wait(lock, [this]() { return _released || _stopping; });
  }

private:
  void _notify_release() {
    {
      std::lock_guard lock{_release_mutex};
      _released = true;
    }
    _release_cv.notify_all();
  }

  void _serve() {
    beast::error_code ec;
    asio::ip::tcp::socket socket{_io_context};
    _acceptor.accept(socket, ec);
    if (ec) return;

    try {
      server_ws ws{std::move(socket), _ssl_ctx};
      ws.next_layer().handshake(asio::ssl::stream_base::server);
      ws.accept();
      _handler(*this, ws);
    } catch (...) {
      // Expected when the client closes or cancels the socket.
    }
  }

  asio::io_context _io_context;
  asio::ssl::context _ssl_ctx;
  asio::ip::tcp::acceptor _acceptor;
  uint16_t _port = 0;
  session_handler _handler;
  std::atomic_bool _stopping = false;
  std::mutex _release_mutex;
  std::condition_variable _release_cv;
  bool _released = false;
  std::jthread _thread;
};

TEST(WebsocketTest, ConfiguresKeepaliveAndRoundTripsMessages) {
  testing::self_signed_cert cert = testing::generate_self_signed_cert();
  loopback_tls_server server{cert, [](loopback_tls_server&, server_ws& ws) {
                               beast::flat_buffer buffer;
                               ws.read(buffer);
                               ws.text(ws.got_text());
                               ws.write(buffer.data());
                             }};

  websocket_options options{
      .connect_timeout = 5s,
      .read_timeout = 5s,
      .write_timeout = 5s,
      .enable_keepalive = true,
      .keepalive_idle_seconds = 12,
      .keepalive_interval_seconds = 4,
      .keepalive_count = 3,
      .ca_cert_pem = cert.cert_pem,
  };
  std::unique_ptr<websocket> ws = make_websocket(
      {.service = std::to_string(server.port()),
       .host = "127.0.0.1",
       .target = "/ws"},
      options);
  ws->stream().text(true);

  asio::ip::tcp::socket& tcp_socket =
      beast::get_lowest_layer(ws->stream()).socket();
  asio::socket_base::keep_alive keepalive_option;
  tcp_socket.get_option(keepalive_option);
  EXPECT_TRUE(keepalive_option.value());

  int native_fd = tcp_socket.native_handle();
  int idle = 0;
  socklen_t opt_len = sizeof(idle);
  ASSERT_EQ(
      ::getsockopt(native_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, &opt_len), 0);
  EXPECT_EQ(idle, 12);

  int interval = 0;
  opt_len = sizeof(interval);
  ASSERT_EQ(
      ::getsockopt(native_fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, &opt_len),
      0);
  EXPECT_EQ(interval, 4);

  int count = 0;
  opt_len = sizeof(count);
  ASSERT_EQ(
      ::getsockopt(native_fd, IPPROTO_TCP, TCP_KEEPCNT, &count, &opt_len), 0);
  EXPECT_EQ(count, 3);

  ws->write(R"({"hello":"world"})");
  EXPECT_EQ(ws->read(), R"({"hello":"world"})");
  ws->close();
}

TEST(WebsocketTest, InvalidKeepaliveOptionThrowsSystemError) {
  testing::self_signed_cert cert = testing::generate_self_signed_cert();
  loopback_tls_server server{cert, [](loopback_tls_server& srv, server_ws&) {
                               srv.wait_until_released();
                             }};

  websocket_options options{
      .connect_timeout = 5s,
      .enable_keepalive = true,
      .keepalive_idle_seconds = 100000,
      .ca_cert_pem = cert.cert_pem,
  };
  EXPECT_THROW(
      (void)make_websocket(
          {.service = std::to_string(server.port()),
           .host = "127.0.0.1",
           .target = "/ws"},
          options),
      std::system_error);

  server.release_session();
}

TEST(WebsocketTest, SilentPeerTriggersReadTimeout) {
  testing::self_signed_cert cert = testing::generate_self_signed_cert();
  loopback_tls_server server{cert, [](loopback_tls_server& srv, server_ws&) {
                               // Hold the socket open silently without sending
                               // any frames.
                               srv.wait_until_released();
                             }};

  websocket_options options{
      .connect_timeout = 5s,
      .read_timeout = 150ms,
      .write_timeout = 5s,
      .ca_cert_pem = cert.cert_pem,
  };
  std::unique_ptr<websocket> ws = make_websocket(
      {.service = std::to_string(server.port()),
       .host = "127.0.0.1",
       .target = "/ws"},
      options);

  const std::chrono::steady_clock::time_point started =
      std::chrono::steady_clock::now();
  try {
    (void)ws->read();
    FAIL() << "Expected read() to time out against a silent peer.";
  } catch (const beast::system_error& err) {
    EXPECT_EQ(err.code(), beast::error::timeout);
  }
  const std::chrono::steady_clock::duration elapsed =
      std::chrono::steady_clock::now() - started;
  EXPECT_GE(elapsed, 100ms);
  EXPECT_LT(elapsed, 2s);

  server.release_session();
}

TEST(WebsocketTest, CancelFromAnotherThreadUnblocksSilentReadImmediately) {
  testing::self_signed_cert cert = testing::generate_self_signed_cert();
  loopback_tls_server server{cert, [](loopback_tls_server& srv, server_ws&) {
                               srv.wait_until_released();
                             }};

  websocket_options options{
      .connect_timeout = 5s,
      .read_timeout = 30s,
      .write_timeout = 5s,
      .ca_cert_pem = cert.cert_pem,
  };
  std::unique_ptr<websocket> ws = make_websocket(
      {.service = std::to_string(server.port()),
       .host = "127.0.0.1",
       .target = "/ws"},
      options);

  std::jthread canceller([&ws]() {
    std::this_thread::sleep_for(50ms);
    ws->cancellation().cancel();
  });

  const std::chrono::steady_clock::time_point started =
      std::chrono::steady_clock::now();
  EXPECT_THROW((void)ws->read(), beast::system_error);
  const std::chrono::steady_clock::duration elapsed =
      std::chrono::steady_clock::now() - started;
  EXPECT_LT(elapsed, 2s);
  EXPECT_TRUE(ws->cancellation().is_cancelled());

  server.release_session();
}

TEST(
    WebsocketTest,
    ParentCancellationFromAnotherThreadUnblocksSilentReadImmediately) {
  testing::self_signed_cert cert = testing::generate_self_signed_cert();
  loopback_tls_server server{cert, [](loopback_tls_server& srv, server_ws&) {
                               srv.wait_until_released();
                             }};

  cancellation parent_cancel;
  websocket_options options{
      .connect_timeout = 5s,
      .read_timeout = 30s,
      .write_timeout = 5s,
      .ca_cert_pem = cert.cert_pem,
      .parent_cancellation = parent_cancel,
  };
  std::unique_ptr<websocket> ws = make_websocket(
      {.service = std::to_string(server.port()),
       .host = "127.0.0.1",
       .target = "/ws"},
      options);

  std::jthread canceller([&parent_cancel]() {
    std::this_thread::sleep_for(50ms);
    parent_cancel.cancel();
  });

  const std::chrono::steady_clock::time_point started =
      std::chrono::steady_clock::now();
  EXPECT_THROW((void)ws->read(), beast::system_error);
  const std::chrono::steady_clock::duration elapsed =
      std::chrono::steady_clock::now() - started;
  EXPECT_LT(elapsed, 2s);
  EXPECT_TRUE(ws->cancellation().is_cancelled());

  server.release_session();
}

} // namespace
} // namespace howling::net
