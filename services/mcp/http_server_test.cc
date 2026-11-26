#include "services/mcp/http_server.h"

#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/read.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/http/verb.hpp"
#include "boost/beast/http/write.hpp"
#include "services/mcp/dispatcher.h"
#include "services/mcp/resource_handler.h"
#include "services/mcp/tool_handler.h"
#include "strings/json.h"
#include "gtest/gtest.h"
#include "json/value.h"

namespace howling::mcp {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;

class echo_tool : public tool_handler {
public:
  std::string_view name() const override { return "echo"; }
  std::string_view description() const override { return "Echoes message."; }
  Json::Value input_schema() const override {
    Json::Value schema(Json::objectValue);
    schema["type"] = "object";
    return schema;
  }
  tool_result call(const Json::Value& arguments) override {
    return tool_result::ok(arguments["message"].asString());
  }
};

class static_resource : public resource_handler {
public:
  std::string_view uri() const override { return "howling://status-info"; }
  std::string_view name() const override { return "StatusInfo"; }
  std::string_view description() const override {
    return "Static server status resource.";
  }
  std::string_view mime_type() const override { return "application/json"; }
  std::string read() override { return R"({"ready":true})"; }
};

class blocking_tool : public tool_handler {
public:
  blocking_tool(std::promise<void>& started, std::shared_future<void> release)
      : _started(started), _release(std::move(release)) {}

  std::string_view name() const override { return "slow"; }
  std::string_view description() const override {
    return "Blocks until freed.";
  }
  Json::Value input_schema() const override {
    Json::Value schema(Json::objectValue);
    schema["type"] = "object";
    return schema;
  }
  tool_result call(const Json::Value&) override {
    _started.set_value();
    _release.wait();
    return tool_result::ok("unblocked");
  }

private:
  std::promise<void>& _started;
  std::shared_future<void> _release;
};

class test_client {
public:
  explicit test_client(unsigned short port) : _stream(_io_context) {
    asio::ip::tcp::resolver resolver(_io_context);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
    _stream.connect(endpoints);
  }

  ~test_client() {
    beast::error_code ignore_error;
    _stream.socket().shutdown(
        asio::ip::tcp::socket::shutdown_both, ignore_error);
  }

  http::response<http::string_body>
  send(http::request<http::string_body> request) {
    request.set(http::field::host, "127.0.0.1");
    request.prepare_payload();
    http::write(_stream, request);

    http::response<http::string_body> response;
    http::read(_stream, _buffer, response);
    return response;
  }

  void send_without_reading(http::request<http::string_body> request) {
    request.set(http::field::host, "127.0.0.1");
    request.prepare_payload();
    http::write(_stream, request);
  }

private:
  asio::io_context _io_context;
  beast::tcp_stream _stream;
  beast::flat_buffer _buffer;
};

struct running_server {
  explicit running_server(
      const mcp_dispatcher& dispatcher, http_server_options options = {})
      : server(io_context, dispatcher, std::move(options)) {
    server.start();
    io_thread = std::jthread([this]() { io_context.run(); });
  }

  ~running_server() {
    server.stop();
    io_context.stop();
    if (io_thread.joinable()) io_thread.join();
  }

  unsigned short port() const { return server.bound_port(); }

  asio::io_context io_context;
  mcp_http_server server;
  std::jthread io_thread;
};

TEST(OriginValidationTest, AllowsOnlyLoopbackOrigins) {
  EXPECT_TRUE(is_allowed_loopback_origin("http://localhost"));
  EXPECT_TRUE(is_allowed_loopback_origin("http://localhost:3000"));
  EXPECT_TRUE(is_allowed_loopback_origin("https://localhost:8443"));
  EXPECT_TRUE(is_allowed_loopback_origin("HTTP://LOCALHOST:8080"));
  EXPECT_TRUE(is_allowed_loopback_origin("http://127.0.0.1"));
  EXPECT_TRUE(is_allowed_loopback_origin("http://127.0.0.1:9090"));
  EXPECT_TRUE(is_allowed_loopback_origin("https://127.0.0.1:443"));
  EXPECT_TRUE(is_allowed_loopback_origin("http://[::1]"));
  EXPECT_TRUE(is_allowed_loopback_origin("http://[::1]:8080"));
  EXPECT_TRUE(is_allowed_loopback_origin("https://[::1]:8443"));

  EXPECT_FALSE(is_allowed_loopback_origin(""));
  EXPECT_FALSE(is_allowed_loopback_origin("null"));
  EXPECT_FALSE(is_allowed_loopback_origin("http://evil.com"));
  EXPECT_FALSE(is_allowed_loopback_origin("http://localhost.evil.com"));
  EXPECT_FALSE(is_allowed_loopback_origin("http://127.0.0.1.evil.com"));
  EXPECT_FALSE(is_allowed_loopback_origin("http://localhost@evil.com"));
  EXPECT_FALSE(is_allowed_loopback_origin("http://localhost/path"));
  EXPECT_FALSE(is_allowed_loopback_origin("http://localhost:99999"));
  EXPECT_FALSE(is_allowed_loopback_origin("ftp://localhost"));
  EXPECT_FALSE(is_allowed_loopback_origin("http://[::1]evil.com"));
}

class McpHttpServerTest : public ::testing::Test {
protected:
  McpHttpServerTest() {
    dispatcher.register_tool(std::make_unique<echo_tool>());
    dispatcher.register_resource(std::make_unique<static_resource>());
  }

  mcp_dispatcher dispatcher{
      server_info{.name = "test-mcp", .version = "0.1.0"}};
};

TEST_F(McpHttpServerTest, RejectsInvalidOptions) {
  asio::io_context io_context;

  http_server_options zero_workers;
  zero_workers.worker_threads = 0;
  EXPECT_THROW(
      mcp_http_server(io_context, dispatcher, zero_workers),
      std::invalid_argument);

  http_server_options zero_body_limit;
  zero_body_limit.max_body_bytes = 0;
  EXPECT_THROW(
      mcp_http_server(io_context, dispatcher, zero_body_limit),
      std::invalid_argument);

  http_server_options zero_timeout;
  zero_timeout.request_timeout = std::chrono::seconds(0);
  EXPECT_THROW(
      mcp_http_server(io_context, dispatcher, zero_timeout),
      std::invalid_argument);

  http_server_options negative_timeout;
  negative_timeout.request_timeout = std::chrono::seconds(-5);
  EXPECT_THROW(
      mcp_http_server(io_context, dispatcher, negative_timeout),
      std::invalid_argument);

  http_server_options hostname_bind_address;
  hostname_bind_address.bind_address = "localhost";
  EXPECT_THROW(
      mcp_http_server(io_context, dispatcher, hostname_bind_address),
      std::exception);
}

TEST_F(McpHttpServerTest, PostMcpDispatchesJsonRpcBatchesAndNotifications) {
  running_server server(dispatcher);
  // Duplicate start() calls must be safe no-ops.
  server.server.start();
  test_client client(server.port());

  http::request<http::string_body> initialize_request{
      http::verb::post, "/mcp", 11};
  initialize_request.set(http::field::content_type, "application/json");
  initialize_request.set(
      http::field::accept, "application/json, text/event-stream");
  initialize_request.body() =
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";

  const http::response<http::string_body> initialize_response =
      client.send(initialize_request);
  EXPECT_EQ(initialize_response.result(), http::status::ok);
  EXPECT_EQ(initialize_response[http::field::content_type], "application/json");
  const Json::Value parsed = to_json(initialize_response.body());
  EXPECT_EQ(parsed["result"]["protocolVersion"].asString(), "2025-03-26");

  http::request<http::string_body> notification_request{
      http::verb::post, "/mcp", 11};
  notification_request.set(http::field::content_type, "application/json");
  notification_request.body() =
      R"({"jsonrpc":"2.0","method":"notifications/initialized"})";

  const http::response<http::string_body> notification_response =
      client.send(notification_request);
  EXPECT_EQ(notification_response.result(), http::status::accepted);
  EXPECT_TRUE(notification_response.body().empty());

  http::request<http::string_body> batch_request{http::verb::post, "/mcp", 11};
  batch_request.set(http::field::content_type, "application/json");
  batch_request.body() =
      R"([{"jsonrpc":"2.0","id":10,"method":"ping"},{"jsonrpc":"2.0","method":"notifications/initialized"}])";
  const http::response<http::string_body> batch_response =
      client.send(batch_request);
  EXPECT_EQ(batch_response.result(), http::status::ok);
  const Json::Value batch_json = to_json(batch_response.body());
  ASSERT_TRUE(batch_json.isArray());
  ASSERT_EQ(batch_json.size(), 1u);
  EXPECT_EQ(batch_json[0]["id"].asInt(), 10);
}

TEST_F(McpHttpServerTest, PostMcpReadsRegisteredResourceAndReportsNotFound) {
  running_server server(dispatcher);
  test_client client(server.port());

  http::request<http::string_body> read_request{http::verb::post, "/mcp", 11};
  read_request.set(http::field::content_type, "application/json");
  read_request.body() =
      R"({"jsonrpc":"2.0","id":1,"method":"resources/read","params":{"uri":"howling://status-info"}})";
  const http::response<http::string_body> read_response =
      client.send(read_request);
  ASSERT_EQ(read_response.result(), http::status::ok);
  const Json::Value read_json = to_json(read_response.body());
  EXPECT_EQ(
      read_json["result"]["contents"][0]["text"].asString(),
      R"({"ready":true})");

  http::request<http::string_body> missing_request{
      http::verb::post, "/mcp", 11};
  missing_request.set(http::field::content_type, "application/json");
  missing_request.body() =
      R"({"jsonrpc":"2.0","id":2,"method":"resources/read","params":{"uri":"howling://missing"}})";
  const http::response<http::string_body> missing_response =
      client.send(missing_request);
  ASSERT_EQ(missing_response.result(), http::status::ok);
  const Json::Value missing_json = to_json(missing_response.body());
  EXPECT_EQ(missing_json["error"]["code"].asInt(), MCP_RESOURCE_NOT_FOUND);
}

TEST_F(McpHttpServerTest, GetAndDeleteMcpReturn405WithAllowPost) {
  running_server server(dispatcher);
  test_client client(server.port());

  const http::response<http::string_body> get_response = client.send(
      http::request<http::string_body>{http::verb::get, "/mcp", 11});
  EXPECT_EQ(get_response.result(), http::status::method_not_allowed);
  EXPECT_EQ(get_response[http::field::allow], "POST");

  const http::response<http::string_body> delete_response = client.send(
      http::request<http::string_body>{http::verb::delete_, "/mcp", 11});
  EXPECT_EQ(delete_response.result(), http::status::method_not_allowed);
  EXPECT_EQ(delete_response[http::field::allow], "POST");
}

TEST_F(McpHttpServerTest, StatusAndUnknownPathEndpoints) {
  running_server server(dispatcher);
  test_client client(server.port());

  const http::response<http::string_body> status_response = client.send(
      http::request<http::string_body>{http::verb::get, "/status", 11});
  EXPECT_EQ(status_response.result(), http::status::ok);
  EXPECT_EQ(status_response[http::field::content_type], "application/json");
  EXPECT_EQ(status_response.body(), R"({"status":"ok"})");

  const http::response<http::string_body> missing_response = client.send(
      http::request<http::string_body>{http::verb::get, "/unknown", 11});
  EXPECT_EQ(missing_response.result(), http::status::not_found);
}

TEST_F(McpHttpServerTest, OriginHeaderEnforcesLoopbackOnlyAndRejectsMultiple) {
  running_server server(dispatcher);
  test_client client(server.port());

  const std::string ping_body = R"({"jsonrpc":"2.0","id":1,"method":"ping"})";

  // Omitted Origin header is allowed.
  {
    http::request<http::string_body> request{http::verb::post, "/mcp", 11};
    request.body() = ping_body;
    EXPECT_EQ(client.send(request).result(), http::status::ok);
  }

  // Loopback origins are allowed.
  const std::vector<std::string> allowed_origins = {
      "http://localhost",
      "http://localhost:3000",
      "https://localhost:8443",
      "http://127.0.0.1:8080",
      "http://[::1]:9000",
  };
  for (const std::string& origin : allowed_origins) {
    http::request<http::string_body> request{http::verb::post, "/mcp", 11};
    request.set(http::field::origin, origin);
    request.body() = ping_body;
    EXPECT_EQ(client.send(request).result(), http::status::ok) << origin;
  }

  // Non-loopback origins are rejected with 403 Forbidden.
  const std::vector<std::string> rejected_origins = {
      "https://evil.example.com",
      "http://localhost.evil.com",
      "http://127.0.0.1.evil.com",
      "null",
  };
  for (const std::string& origin : rejected_origins) {
    http::request<http::string_body> request{http::verb::post, "/mcp", 11};
    request.set(http::field::origin, origin);
    request.body() = ping_body;
    EXPECT_EQ(client.send(request).result(), http::status::forbidden) << origin;
  }

  // Multiple Origin headers must always be rejected.
  {
    http::request<http::string_body> duplicate_origin_request{
        http::verb::post, "/mcp", 11};
    duplicate_origin_request.insert(http::field::origin, "http://localhost");
    duplicate_origin_request.insert(
        http::field::origin, "https://evil.example.com");
    duplicate_origin_request.body() = ping_body;
    EXPECT_EQ(
        client.send(duplicate_origin_request).result(),
        http::status::forbidden);
  }
}

TEST_F(McpHttpServerTest, AcceptHeaderNegotiation) {
  running_server server(dispatcher);
  test_client client(server.port());

  const std::string ping_body = R"({"jsonrpc":"2.0","id":1,"method":"ping"})";
  const std::vector<std::string> valid_accepts = {
      "application/json",
      "application/*",
      "*/*",
      "application/json, text/event-stream",
      "text/html;q=0.5, application/json;q=0.9",
  };
  for (const std::string& accept : valid_accepts) {
    http::request<http::string_body> request{http::verb::post, "/mcp", 11};
    request.set(http::field::accept, accept);
    request.body() = ping_body;
    EXPECT_EQ(client.send(request).result(), http::status::ok) << accept;
  }

  // Multiple Accept headers are evaluated together.
  {
    http::request<http::string_body> multi_accept_request{
        http::verb::post, "/mcp", 11};
    multi_accept_request.insert(http::field::accept, "text/html");
    multi_accept_request.insert(http::field::accept, "application/json");
    multi_accept_request.body() = ping_body;
    EXPECT_EQ(client.send(multi_accept_request).result(), http::status::ok);
  }

  const std::vector<std::string> rejected_accepts = {
      "text/html",
      "text/event-stream",
      "application/json;q=0",
      "application/json;q=0.0",
      "application/json;q=0.000",
      "application/json;q=0, */*;q=1",
      "*/*;q=0",
  };
  for (const std::string& accept : rejected_accepts) {
    http::request<http::string_body> request{http::verb::post, "/mcp", 11};
    request.set(http::field::accept, accept);
    request.body() = ping_body;
    EXPECT_EQ(client.send(request).result(), http::status::not_acceptable)
        << accept;
  }
}

TEST_F(McpHttpServerTest, KeepAliveSupportsSequentialRequestsAndLargeBodies) {
  running_server server(dispatcher);
  test_client client(server.port());

  // Send a payload larger than 8 KB to verify no 8 KB flat_buffer cap exists.
  const std::string large_message(16384, 'x');
  Json::Value call_request(Json::objectValue);
  call_request["jsonrpc"] = "2.0";
  call_request["id"] = 42;
  call_request["method"] = "tools/call";
  call_request["params"]["name"] = "echo";
  call_request["params"]["arguments"]["message"] = large_message;

  for (int i = 0; i < 3; ++i) {
    http::request<http::string_body> request{http::verb::post, "/mcp", 11};
    request.keep_alive(true);
    request.set(http::field::content_type, "application/json");
    request.body() = to_string(call_request);

    const http::response<http::string_body> response = client.send(request);
    ASSERT_EQ(response.result(), http::status::ok);
    const Json::Value parsed = to_json(response.body());
    EXPECT_EQ(parsed["result"]["content"][0]["text"].asString(), large_message);
  }
}

TEST_F(McpHttpServerTest, BodyExceedingLimitReturnsPayloadTooLarge) {
  http_server_options options;
  options.max_body_bytes = 128;
  running_server server(dispatcher, options);
  test_client client(server.port());

  http::request<http::string_body> request{http::verb::post, "/mcp", 11};
  request.body() = std::string(512, 'a');

  const http::response<http::string_body> response = client.send(request);
  EXPECT_EQ(response.result(), http::status::payload_too_large);
}

TEST_F(McpHttpServerTest, SlowToolHandlerDoesNotBlockStatusEndpoint) {
  std::promise<void> started_promise;
  std::future<void> started_future = started_promise.get_future();
  std::promise<void> release_promise;
  std::shared_future<void> release_future =
      release_promise.get_future().share();

  dispatcher.register_tool(
      std::make_unique<blocking_tool>(started_promise, release_future));

  http_server_options options;
  options.worker_threads = 1;
  running_server server(dispatcher, options);

  std::future<http::response<http::string_body>> slow_response =
      std::async(std::launch::async, [port = server.port()]() {
        test_client slow_client(port);
        http::request<http::string_body> request{http::verb::post, "/mcp", 11};
        request.set(http::field::content_type, "application/json");
        request.body() =
            R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"slow"}})";
        return slow_client.send(request);
      });

  // Wait until the single worker thread is actively blocked inside `slow`.
  ASSERT_EQ(
      started_future.wait_for(std::chrono::seconds(5)),
      std::future_status::ready);

  // Even with all worker threads blocked, GET /status must respond immediately.
  test_client status_client(server.port());
  const http::response<http::string_body> status_response = status_client.send(
      http::request<http::string_body>{http::verb::get, "/status", 11});
  EXPECT_EQ(status_response.result(), http::status::ok);
  EXPECT_EQ(status_response.body(), R"({"status":"ok"})");

  // Unblock the slow tool and verify its response completes cleanly.
  release_promise.set_value();
  ASSERT_EQ(
      slow_response.wait_for(std::chrono::seconds(5)),
      std::future_status::ready);
  const http::response<http::string_body> slow_result = slow_response.get();
  EXPECT_EQ(slow_result.result(), http::status::ok);
  const Json::Value parsed = to_json(slow_result.body());
  EXPECT_EQ(parsed["result"]["content"][0]["text"].asString(), "unblocked");
}

TEST_F(McpHttpServerTest, StopClosesOpenKeepAliveClientSoIoContextExits) {
  asio::io_context io_context;
  mcp_http_server server(io_context, dispatcher);
  server.start();

  std::promise<void> io_finished_promise;
  std::future<void> io_finished_future = io_finished_promise.get_future();
  std::jthread io_thread([&io_context, &io_finished_promise]() {
    io_context.run();
    io_finished_promise.set_value();
  });

  // Keep a persistent HTTP/1.1 connection open after completing a request.
  test_client keep_alive_client(server.bound_port());
  http::request<http::string_body> ping_request{http::verb::post, "/mcp", 11};
  ping_request.keep_alive(true);
  ping_request.body() = R"({"jsonrpc":"2.0","id":1,"method":"ping"})";
  ASSERT_EQ(keep_alive_client.send(ping_request).result(), http::status::ok);

  // Stopping the server while io_context is still running must close the open
  // keep-alive session so io_context.run() exits without waiting for timeout.
  server.stop();
  EXPECT_EQ(
      io_finished_future.wait_for(std::chrono::seconds(5)),
      std::future_status::ready);
}

TEST_F(
    McpHttpServerTest, StopWaitsForInFlightToolCallAndBlocksConcurrentStops) {
  std::promise<void> started_promise;
  std::future<void> started_future = started_promise.get_future();
  std::promise<void> release_promise;
  std::shared_future<void> release_future =
      release_promise.get_future().share();

  dispatcher.register_tool(
      std::make_unique<blocking_tool>(started_promise, release_future));

  http_server_options options;
  options.worker_threads = 1;
  asio::io_context io_context;
  mcp_http_server server(io_context, dispatcher, options);
  server.start();
  std::jthread io_thread([&io_context]() { io_context.run(); });

  test_client slow_client(server.bound_port());
  http::request<http::string_body> slow_request{http::verb::post, "/mcp", 11};
  slow_request.set(http::field::content_type, "application/json");
  slow_request.body() =
      R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"slow"}})";
  slow_client.send_without_reading(slow_request);

  ASSERT_EQ(
      started_future.wait_for(std::chrono::seconds(5)),
      std::future_status::ready);

  // Start multiple concurrent stop() callers while the worker thread is busy.
  std::vector<std::future<void>> stop_futures;
  stop_futures.reserve(3);
  for (int i = 0; i < 3; ++i) {
    stop_futures.push_back(
        std::async(std::launch::async, [&server]() { server.stop(); }));
  }

  // Every concurrent stop() caller must remain blocked while the tool runs.
  for (const std::future<void>& stop_future : stop_futures) {
    EXPECT_EQ(
        stop_future.wait_for(std::chrono::milliseconds(50)),
        std::future_status::timeout);
  }

  release_promise.set_value();

  for (std::future<void>& stop_future : stop_futures) {
    EXPECT_EQ(
        stop_future.wait_for(std::chrono::seconds(5)),
        std::future_status::ready);
  }

  io_context.stop();
}

} // namespace
} // namespace howling::mcp
