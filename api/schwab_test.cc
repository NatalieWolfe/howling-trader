#include "api/schwab.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "api/schwab/connect.h"
#include "boost/asio/buffer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/websocket.hpp"
#include "boost/beast/websocket/ssl.hpp"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "functional/retry.h"
#include "net/connect.h"
#include "net/test_tls_cert.h"
#include "strings/json.h"
#include "gtest/gtest.h"
#include "json/json.h"

namespace howling::schwab {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
using namespace ::std::chrono_literals;

using server_ws =
    beast::websocket::stream<asio::ssl::stream<asio::ip::tcp::socket>>;

std::string serialize_json(const Json::Value& value) {
  Json::StreamWriterBuilder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "";
  return Json::writeString(builder, value);
}

Json::Value read_json_frame(server_ws& ws) {
  beast::flat_buffer buffer;
  ws.read(buffer);
  return to_json(beast::buffers_to_string(buffer.cdata()));
}

void write_json_frame(server_ws& ws, const Json::Value& value) {
  ws.text(true);
  ws.write(asio::buffer(serialize_json(value)));
}

Json::Value make_response_envelope(
    const Json::Value& request, int code = 0, std::string_view message = "OK") {
  Json::Value entry{Json::objectValue};
  entry["service"] = request["service"];
  entry["requestid"] = request["requestid"];
  entry["command"] = request["command"];
  entry["content"] = Json::objectValue;
  entry["content"]["code"] = code;
  entry["content"]["msg"] = std::string{message};

  Json::Value root{Json::objectValue};
  root["response"] = Json::arrayValue;
  root["response"].append(std::move(entry));
  return root;
}

Json::Value
make_chart_message(std::string_view symbol_key, double close_price) {
  Json::Value candle{Json::objectValue};
  candle["key"] = std::string{symbol_key};
  candle["1"] = 1;
  candle["2"] = close_price - 1.0;
  candle["3"] = close_price + 1.0;
  candle["4"] = close_price - 2.0;
  candle["5"] = close_price;
  candle["6"] = 100.0;
  candle["7"] = Json::Int64{1700000040000};

  Json::Value datum{Json::objectValue};
  datum["service"] = "CHART_EQUITY";
  datum["content"] = Json::arrayValue;
  datum["content"].append(std::move(candle));

  Json::Value root{Json::objectValue};
  root["data"] = Json::arrayValue;
  root["data"].append(std::move(datum));
  return root;
}

Json::Value make_heartbeat_message() {
  Json::Value notify{Json::objectValue};
  notify["heartbeat"] = "1700000000000";
  Json::Value root{Json::objectValue};
  root["notify"] = Json::arrayValue;
  root["notify"].append(std::move(notify));
  return root;
}

class mock_schwab_server {
public:
  using session_handler =
      std::function<void(int, mock_schwab_server&, server_ws&)>;

  mock_schwab_server(
      const net::testing::self_signed_cert& cert, session_handler handler)
      : _ssl_ctx{asio::ssl::context::tls_server},
        _acceptor{
            _io_context,
            asio::ip::tcp::endpoint{asio::ip::make_address("127.0.0.1"), 0}},
        _handler{std::move(handler)} {
    _ssl_ctx.use_certificate_chain(asio::buffer(cert.cert_pem));
    _ssl_ctx.use_private_key(
        asio::buffer(cert.key_pem), asio::ssl::context::pem);
    _port = _acceptor.local_endpoint().port();
    _start_accept();
    _thread = std::jthread([this]() { _io_context.run(); });
  }

  ~mock_schwab_server() {
    stop();
    if (_thread.joinable()) _thread.join();
    _session_threads.clear();
  }

  void stop() {
    {
      std::lock_guard lock{_mutex};
      if (_stopping) return;
      _stopping = true;
    }
    _release_cv.notify_all();
    asio::post(_io_context, [this]() {
      beast::error_code ec;
      _acceptor.close(ec);
    });
  }

  [[nodiscard]] uint16_t port() const { return _port; }

  [[nodiscard]] bool is_stopping() const {
    std::lock_guard lock{_mutex};
    return _stopping;
  }

  void wait_until_stopped_or(std::chrono::milliseconds duration) {
    std::unique_lock lock{_mutex};
    _release_cv.wait_for(lock, duration, [this]() { return _stopping; });
  }

private:
  void _start_accept() {
    _acceptor.async_accept(
        [this](beast::error_code ec, asio::ip::tcp::socket socket) {
          if (ec || is_stopping()) return;
          int current_index = _connection_index++;
          _session_threads.emplace_back(
              [this, current_index, sock = std::move(socket)]() mutable {
                try {
                  server_ws ws{std::move(sock), _ssl_ctx};
                  ws.next_layer().handshake(asio::ssl::stream_base::server);
                  ws.accept();
                  _handler(current_index, *this, ws);
                } catch (...) {
                  // Expected when the client disconnects, times out, or stops.
                }
              });
          _start_accept();
        });
  }

  asio::io_context _io_context;
  asio::ssl::context _ssl_ctx;
  asio::ip::tcp::acceptor _acceptor;
  uint16_t _port = 0;
  int _connection_index = 0;
  session_handler _handler;
  mutable std::mutex _mutex;
  std::condition_variable _release_cv;
  bool _stopping = false;
  std::vector<std::jthread> _session_threads;
  std::jthread _thread;
};

streamer_session_info
make_test_session(uint16_t port, std::string bearer_token) {
  return streamer_session_info{
      .socket_url =
          {.service = std::to_string(port),
           .host = "127.0.0.1",
           .target = "/ws"},
      .customer_id = "cust-1",
      .correlation_id = "corr-1",
      .channel = "N9",
      .function_id = "APIAPP",
      .bearer_token = std::move(bearer_token),
  };
}

TEST(SchwabStreamTest, SilentPeerTriggersTimeoutAndAutoReconnects) {
  net::testing::self_signed_cert cert =
      net::testing::generate_self_signed_cert();

  std::mutex recorded_mutex;
  std::vector<std::string> login_tokens;
  std::vector<std::string> subscribed_services;
  std::vector<double> received_closes;
  std::condition_variable data_cv;

  mock_schwab_server server{
      cert, [&](int conn_index, mock_schwab_server& srv, server_ws& ws) {
        Json::Value login = read_json_frame(ws);
        ASSERT_EQ(login["service"].asString(), "ADMIN");
        ASSERT_EQ(login["command"].asString(), "LOGIN");
        {
          std::lock_guard lock{recorded_mutex};
          login_tokens.push_back(
              login["parameters"]["Authorization"].asString());
        }
        write_json_frame(ws, make_response_envelope(login));

        // Receive CHART_EQUITY and LEVELONE_EQUITIES ADD commands.
        for (int i = 0; i < 2; ++i) {
          Json::Value sub = read_json_frame(ws);
          {
            std::lock_guard lock{recorded_mutex};
            subscribed_services.push_back(
                sub["service"].asString() + ":" +
                sub["parameters"]["keys"].asString());
          }
          write_json_frame(ws, make_response_envelope(sub));
        }

        if (conn_index == 0) {
          write_json_frame(ws, make_chart_message("AAPL", 101.5));
          // Simulate a half-open socket: stay silent past read_timeout.
          srv.wait_until_stopped_or(5s);
        } else {
          write_json_frame(ws, make_chart_message("AAPL", 202.5));
          srv.wait_until_stopped_or(5s);
        }
      }};

  std::atomic_int token_counter = 0;
  scoped_streamer_session_provider session_guard{[&]() {
    int next_id = ++token_counter;
    return make_test_session(server.port(), "token-" + std::to_string(next_id));
  }};

  stream_options options{
      .websocket_options =
          {.connect_timeout = 5s,
           .read_timeout = 150ms,
           .write_timeout = 5s,
           .ca_cert_pem = cert.cert_pem},
      .reconnect_backoff =
          {.initial_delay = 20ms,
           .maximum_delay = 100ms,
           .deadline = std::chrono::steady_clock::duration::max(),
           .exponent = 2.0},
  };

  stream schwab_stream{std::move(options)};
  schwab_stream.on_chart([&](stock::Symbol, Candle candle) {
    {
      std::lock_guard lock{recorded_mutex};
      received_closes.push_back(candle.close());
    }
    data_cv.notify_all();
  });

  std::jthread worker([&]() {
    schwab_stream.start([&]() { schwab_stream.add_symbol(stock::AAPL); });
  });

  {
    std::unique_lock lock{recorded_mutex};
    ASSERT_TRUE(data_cv.wait_for(
        lock, 5s, [&]() { return received_closes.size() >= 2; }));
  }

  schwab_stream.stop();
  server.stop();

  std::lock_guard lock{recorded_mutex};
  ASSERT_GE(login_tokens.size(), 2u);
  EXPECT_EQ(login_tokens[0], "token-1");
  EXPECT_EQ(login_tokens[1], "token-2");
  EXPECT_EQ(received_closes[0], 101.5);
  EXPECT_EQ(received_closes[1], 202.5);
  // Both connections must have subscribed CHART_EQUITY and LEVELONE_EQUITIES.
  ASSERT_GE(subscribed_services.size(), 4u);
  EXPECT_EQ(subscribed_services[0], "CHART_EQUITY:AAPL");
  EXPECT_EQ(subscribed_services[1], "LEVELONE_EQUITIES:AAPL");
  EXPECT_EQ(subscribed_services[2], "CHART_EQUITY:AAPL");
  EXPECT_EQ(subscribed_services[3], "LEVELONE_EQUITIES:AAPL");
}

TEST(SchwabStreamTest, AbruptDropReconnectsWithBackoffAndResubscribes) {
  net::testing::self_signed_cert cert =
      net::testing::generate_self_signed_cert();

  std::mutex recorded_mutex;
  std::vector<std::string> second_conn_subscriptions;
  std::vector<double> received_closes;
  std::condition_variable data_cv;

  mock_schwab_server server{
      cert, [&](int conn_index, mock_schwab_server& srv, server_ws& ws) {
        Json::Value login = read_json_frame(ws);
        write_json_frame(ws, make_response_envelope(login));

        // Initial connection calls add_symbol twice (4 commands); reconnect
        // batches both symbols into one command per service (2 commands).
        int expected_sub_commands = (conn_index == 0) ? 4 : 2;
        for (int i = 0; i < expected_sub_commands; ++i) {
          Json::Value sub = read_json_frame(ws);
          if (conn_index == 1) {
            std::lock_guard lock{recorded_mutex};
            second_conn_subscriptions.push_back(
                sub["service"].asString() + ":" +
                sub["parameters"]["keys"].asString());
          }
          write_json_frame(ws, make_response_envelope(sub));
        }

        if (conn_index == 0) {
          write_json_frame(ws, make_chart_message("AAPL", 150.0));
          // Drop the TCP socket abruptly without a WebSocket close frame.
          beast::error_code ec;
          ws.next_layer().next_layer().shutdown(
              asio::ip::tcp::socket::shutdown_send, ec);
          ws.next_layer().next_layer().close(ec);
        } else {
          write_json_frame(ws, make_chart_message("NVDA", 350.0));
          srv.wait_until_stopped_or(5s);
        }
      }};

  std::atomic_int provider_calls = 0;
  scoped_streamer_session_provider session_guard{[&]() {
    int call = ++provider_calls;
    // Fail three reconnect attempts to verify exponential backoff retry:
    // 25ms + 50ms + 100ms = 175ms.
    if (call == 2 || call == 3 || call == 4) {
      throw std::runtime_error("Simulated temporary network outage.");
    }
    return make_test_session(server.port(), "token-" + std::to_string(call));
  }};

  stream_options options{
      .websocket_options =
          {.connect_timeout = 5s,
           .read_timeout = 2s,
           .write_timeout = 5s,
           .ca_cert_pem = cert.cert_pem},
      .reconnect_backoff =
          {.initial_delay = 25ms,
           .maximum_delay = 100ms,
           .deadline = std::chrono::steady_clock::duration::max(),
           .exponent = 2.0},
  };

  stream schwab_stream{std::move(options)};
  schwab_stream.on_chart([&](stock::Symbol, Candle candle) {
    {
      std::lock_guard lock{recorded_mutex};
      received_closes.push_back(candle.close());
    }
    data_cv.notify_all();
  });

  auto started = std::chrono::steady_clock::now();
  std::jthread worker([&]() {
    schwab_stream.start([&]() {
      schwab_stream.add_symbol(stock::AAPL);
      schwab_stream.add_symbol(stock::NVDA);
    });
  });

  {
    std::unique_lock lock{recorded_mutex};
    ASSERT_TRUE(data_cv.wait_for(
        lock, 5s, [&]() { return received_closes.size() >= 2; }));
  }
  auto elapsed = std::chrono::steady_clock::now() - started;

  schwab_stream.stop();
  server.stop();

  // Backoff across 3 failed reconnect attempts: 25ms + 50ms + 100ms = 175ms.
  EXPECT_GE(elapsed, 120ms);
  EXPECT_GE(provider_calls.load(), 5);

  std::lock_guard lock{recorded_mutex};
  EXPECT_EQ(received_closes[0], 150.0);
  EXPECT_EQ(received_closes[1], 350.0);
  ASSERT_EQ(second_conn_subscriptions.size(), 2u);
  EXPECT_EQ(second_conn_subscriptions[0], "CHART_EQUITY:AAPL,NVDA");
  EXPECT_EQ(second_conn_subscriptions[1], "LEVELONE_EQUITIES:AAPL,NVDA");
}

TEST(SchwabStreamTest, HeartbeatsKeepConnectionAlivePastReadTimeout) {
  net::testing::self_signed_cert cert =
      net::testing::generate_self_signed_cert();

  std::atomic_int connection_count = 0;
  std::mutex data_mutex;
  std::condition_variable data_cv;
  bool candle_received = false;

  mock_schwab_server server{
      cert, [&](int, mock_schwab_server& srv, server_ws& ws) {
        ++connection_count;
        Json::Value login = read_json_frame(ws);
        write_json_frame(ws, make_response_envelope(login));
        for (int i = 0; i < 2; ++i) {
          Json::Value sub = read_json_frame(ws);
          write_json_frame(ws, make_response_envelope(sub));
        }

        // Send heartbeats faster than the 180ms read timeout for 400ms total.
        for (int i = 0; i < 8 && !srv.is_stopping(); ++i) {
          std::this_thread::sleep_for(50ms);
          write_json_frame(ws, make_heartbeat_message());
        }
        write_json_frame(ws, make_chart_message("AAPL", 123.45));
        srv.wait_until_stopped_or(5s);
      }};

  scoped_streamer_session_provider session_guard{
      [&]() { return make_test_session(server.port(), "token-1"); }};

  stream_options options{
      .websocket_options =
          {.connect_timeout = 5s,
           .read_timeout = 180ms,
           .write_timeout = 5s,
           .ca_cert_pem = cert.cert_pem},
      .reconnect_backoff =
          {.initial_delay = 20ms,
           .maximum_delay = 100ms,
           .deadline = std::chrono::steady_clock::duration::max(),
           .exponent = 2.0},
  };

  stream schwab_stream{std::move(options)};
  schwab_stream.on_chart([&](stock::Symbol, Candle) {
    {
      std::lock_guard lock{data_mutex};
      candle_received = true;
    }
    data_cv.notify_all();
  });

  std::jthread worker([&]() {
    schwab_stream.start([&]() { schwab_stream.add_symbol(stock::AAPL); });
  });

  {
    std::unique_lock lock{data_mutex};
    ASSERT_TRUE(data_cv.wait_for(lock, 5s, [&]() { return candle_received; }));
  }

  schwab_stream.stop();
  server.stop();

  EXPECT_EQ(connection_count.load(), 1);
}

TEST(SchwabStreamTest, StopUnblocksSilentPeerReadWithoutHanging) {
  net::testing::self_signed_cert cert =
      net::testing::generate_self_signed_cert();

  std::mutex ready_mutex;
  std::condition_variable ready_cv;
  bool subscribed = false;

  mock_schwab_server server{
      cert, [&](int, mock_schwab_server& srv, server_ws& ws) {
        Json::Value login = read_json_frame(ws);
        write_json_frame(ws, make_response_envelope(login));
        for (int i = 0; i < 2; ++i) {
          Json::Value sub = read_json_frame(ws);
          write_json_frame(ws, make_response_envelope(sub));
        }
        {
          std::lock_guard lock{ready_mutex};
          subscribed = true;
        }
        ready_cv.notify_all();
        // Remain completely silent with a 30s client read timeout.
        srv.wait_until_stopped_or(30s);
      }};

  scoped_streamer_session_provider session_guard{
      [&]() { return make_test_session(server.port(), "token-1"); }};

  stream_options options{
      .websocket_options =
          {.connect_timeout = 5s,
           .read_timeout = 30s,
           .write_timeout = 5s,
           .ca_cert_pem = cert.cert_pem},
      .reconnect_backoff =
          {.initial_delay = 1s,
           .maximum_delay = 10s,
           .deadline = std::chrono::steady_clock::duration::max(),
           .exponent = 2.0},
  };

  stream schwab_stream{std::move(options)};
  std::jthread worker([&]() {
    schwab_stream.start([&]() { schwab_stream.add_symbol(stock::AAPL); });
  });

  {
    std::unique_lock lock{ready_mutex};
    ASSERT_TRUE(ready_cv.wait_for(lock, 5s, [&]() { return subscribed; }));
  }
  // Allow the worker thread to enter the blocking read on the silent socket.
  std::this_thread::sleep_for(50ms);
  EXPECT_TRUE(schwab_stream.is_running());

  auto stop_started = std::chrono::steady_clock::now();
  schwab_stream.stop();
  auto stop_elapsed = std::chrono::steady_clock::now() - stop_started;

  EXPECT_LT(stop_elapsed, 2s);
  EXPECT_FALSE(schwab_stream.is_running());
  server.stop();
}

TEST(SchwabStreamTest, StopInterruptsReconnectBackoffImmediately) {
  std::mutex attempt_mutex;
  std::condition_variable attempt_cv;
  bool first_attempt_failed = false;

  scoped_streamer_session_provider session_guard{
      [&]() -> streamer_session_info {
        {
          std::lock_guard lock{attempt_mutex};
          first_attempt_failed = true;
        }
        attempt_cv.notify_all();
        throw std::runtime_error("Simulated offline network.");
      }};

  stream_options options{
      .reconnect_backoff =
          {.initial_delay = 30s,
           .maximum_delay = 60s,
           .deadline = std::chrono::steady_clock::duration::max(),
           .exponent = 2.0},
  };

  stream schwab_stream{std::move(options)};
  std::jthread worker([&]() { schwab_stream.start(); });

  {
    std::unique_lock lock{attempt_mutex};
    ASSERT_TRUE(
        attempt_cv.wait_for(lock, 5s, [&]() { return first_attempt_failed; }));
  }
  std::this_thread::sleep_for(50ms);

  auto stop_started = std::chrono::steady_clock::now();
  schwab_stream.stop();
  auto stop_elapsed = std::chrono::steady_clock::now() - stop_started;

  EXPECT_LT(stop_elapsed, 2s);
  EXPECT_FALSE(schwab_stream.is_running());
}

TEST(SchwabStreamTest, StartAfterStopThrowsRuntimeError) {
  stream stopped_stream{stream_options{}};
  EXPECT_NO_THROW(stopped_stream.stop());
  EXPECT_NO_THROW(stopped_stream.stop());
  EXPECT_THROW(stopped_stream.start(), std::runtime_error);
}

TEST(SchwabStreamTest, StopIsIdempotentAndSafeFromChartCallback) {
  net::testing::self_signed_cert cert =
      net::testing::generate_self_signed_cert();
  mock_schwab_server server{
      cert, [&](int, mock_schwab_server& srv, server_ws& ws) {
        Json::Value login = read_json_frame(ws);
        write_json_frame(ws, make_response_envelope(login));
        write_json_frame(ws, make_chart_message("AAPL", 99.0));
        srv.wait_until_stopped_or(5s);
      }};

  scoped_streamer_session_provider session_guard{
      [&]() { return make_test_session(server.port(), "token-1"); }};

  stream_options options{
      .websocket_options =
          {.connect_timeout = 5s,
           .read_timeout = 5s,
           .write_timeout = 5s,
           .ca_cert_pem = cert.cert_pem},
  };

  stream callback_stream{std::move(options)};
  bool invoked = false;
  callback_stream.on_chart([&](stock::Symbol, Candle) {
    invoked = true;
    callback_stream.stop();
  });

  callback_stream.start();
  EXPECT_TRUE(invoked);
  EXPECT_FALSE(callback_stream.is_running());
  server.stop();
}

} // namespace
} // namespace howling::schwab
