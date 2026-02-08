#include "services/oauth/oauth_http_service.h"

#include <chrono>
#include <thread>

#include "boost/asio.hpp"
#include "boost/beast.hpp"
#include "gtest/gtest.h"

namespace howling {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;

using http_response =
    ::boost::beast::http::response<::boost::beast::http::dynamic_body>;
using http_request =
    ::boost::beast::http::request<::boost::beast::http::dynamic_body>;

unsigned short get_port() {
  static unsigned short available_port = 18080;
  return ++available_port;
}

struct test_server {
  test_server() : port{get_port()}, service{ioc, port} {
    service.start();
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
  oauth_http_service service;
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
    // TODO: Throw if ec is not ok.
  }

  asio::io_context ioc;
  beast::tcp_stream stream;
};

TEST(OauthHttpService, CallbackReturnsOk) {
  test_server server;
  test_client client{server.port};

  http::request<http::string_body> req{
      http::verb::get, "/callback?code=test_code", 11};
  req.set(http::field::host, "127.0.0.1");
  req.set(http::field::user_agent, "test-client");

  http::write(client.stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(client.stream, buffer, res);

  EXPECT_EQ(res.result(), http::status::ok);
  EXPECT_TRUE(
      res.body().find("Authentication successful") != std::string::npos);
}

TEST(OauthHttpService, MissingCodeReturnsBadRequest) {
  test_server server;
  test_client client{server.port};

  http::request<http::string_body> req{http::verb::get, "/callback", 11};
  req.set(http::field::host, "127.0.0.1");

  http::write(client.stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(client.stream, buffer, res);

  EXPECT_EQ(res.result(), http::status::bad_request);
}

} // namespace
} // namespace howling
