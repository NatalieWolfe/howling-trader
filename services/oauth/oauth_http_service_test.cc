#include "services/oauth/mock_oauth_exchanger.h"
#include "services/oauth/oauth_http_service.h"

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "api/schwab/oauth.h"
#include "boost/asio.hpp"
#include "boost/beast.hpp"
#include "net/connect.h"
#include "services/database.h"
#include "services/mock_database.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_DECLARE_FLAG(absl::Duration, schwab_auth_timeout);

namespace howling {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;

using ::testing::_;
using ::testing::Return;

using http_response =
    ::boost::beast::http::response<::boost::beast::http::dynamic_body>;
using http_request =
    ::boost::beast::http::request<::boost::beast::http::dynamic_body>;

unsigned short get_port() {
  static unsigned short available_port = 18080;
  return ++available_port;
}

struct test_server {
  test_server() : port{get_port()}, service{} {
    auto oauth_exchanger = std::make_unique<mock_oauth_exchanger>();
    exchanger = oauth_exchanger.get();
    service = std::make_unique<oauth_http_service>(
        ioc, port, db, std::move(oauth_exchanger));
    service->start();
    server_thread = std::jthread([this]() { ioc.run(); });
    // Give the server a moment to start and enter the accept loop.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  ~test_server() {
    ioc.stop();
    if (server_thread.joinable()) server_thread.join();
  }

  asio::io_context ioc;
  unsigned short port;
  mock_database db;
  mock_oauth_exchanger* exchanger;
  std::unique_ptr<oauth_http_service> service;
  std::jthread server_thread;
};

struct test_client {
  explicit test_client(unsigned short port) : stream{ioc} {
    asio::ip::tcp::resolver resolver{ioc};
    auto const results = resolver.resolve("127.0.0.1", std::to_string(port));
    stream.connect(results);
  }

  ~test_client() {
    beast::error_code ec;
    stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  }

  asio::io_context ioc;
  beast::tcp_stream stream;
};

// MARK: GET /schwab/oauth-callback

TEST(SchwabOauthCallback, ReturnsOkAndStoresToken) {
  test_server server;
  test_client client{server.port};

  EXPECT_CALL(*server.exchanger, exchange("test_code"))
      .WillOnce(Return(
          schwab::oauth_tokens{
              .access_token = "access",
              .refresh_token = "refresh",
              .expires_in = 3600}));

  std::promise<void> save_promise;
  std::future<void> save_future = save_promise.get_future();
  EXPECT_CALL(server.db, save_refresh_token("schwab", "refresh"))
      .WillOnce([&](std::string_view, std::string_view) {
        save_promise.set_value();
        std::promise<void> p;
        p.set_value();
        return p.get_future();
      });

  http::request<http::string_body> req{
      http::verb::get, "/schwab/oauth-callback?code=test_code", 11};
  req.set(http::field::host, "127.0.0.1");
  req.set(http::field::user_agent, "test-client");

  http::write(client.stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(client.stream, buffer, res);

  EXPECT_EQ(res.result(), http::status::ok);
  EXPECT_TRUE(
      res.body().find("Authentication successful") != std::string::npos);

  // Wait for the async DB call to complete.
  EXPECT_EQ(
      save_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
}

TEST(SchwabOauthCallback, MissingCodeReturnsBadRequest) {
  test_server server;
  test_client client{server.port};

  http::request<http::string_body> req{
      http::verb::get, "/schwab/oauth-callback", 11};
  req.set(http::field::host, "127.0.0.1");

  http::write(client.stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(client.stream, buffer, res);

  EXPECT_EQ(res.result(), http::status::bad_request);
}

// MARK: GET /status

TEST(Status, ReturnsOk) {
  test_server server;
  test_client client{server.port};

  http::request<http::string_body> req{http::verb::get, "/status", 11};
  req.set(http::field::host, "127.0.0.1");

  http::write(client.stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(client.stream, buffer, res);

  EXPECT_EQ(res.result(), http::status::ok);
  EXPECT_EQ(res.body(), "OK\n");
}

// MARK: GET /schwab/status

TEST(SchwabStatus, ReturnsAuthenticationRequiredWhenNoToken) {
  test_server server;
  test_client client{server.port};

  ::absl::SetFlag(&FLAGS_schwab_auth_timeout, ::absl::Seconds(2));

  http::request<http::string_body> req{http::verb::get, "/schwab/status", 11};
  req.set(http::field::host, "127.0.0.1");

  http::write(client.stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(client.stream, buffer, res);

  EXPECT_EQ(res.result(), http::status::ok);
  EXPECT_EQ(res.body(), "Authentication Required\n");
}

} // namespace
} // namespace howling
