#include "services/mcp/dispatcher.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "services/mcp/resource_handler.h"
#include "services/mcp/tool_handler.h"
#include "gtest/gtest.h"
#include "json/value.h"

namespace howling::mcp {
namespace {

class fake_tool_handler : public tool_handler {
public:
  using callback_type = std::function<tool_result(const Json::Value&)>;

  fake_tool_handler(
      std::string name,
      std::string description,
      Json::Value schema,
      tool_annotations annotations,
      callback_type callback)
      : _name(std::move(name)), _description(std::move(description)),
        _schema(std::move(schema)), _annotations(annotations),
        _callback(std::move(callback)) {}

  std::string_view name() const override { return _name; }
  std::string_view description() const override { return _description; }
  Json::Value input_schema() const override { return _schema; }
  tool_annotations annotations() const override { return _annotations; }
  tool_result call(const Json::Value& arguments) override {
    return _callback(arguments);
  }

private:
  std::string _name;
  std::string _description;
  Json::Value _schema;
  tool_annotations _annotations;
  callback_type _callback;
};

class throwing_schema_tool_handler : public tool_handler {
public:
  std::string_view name() const override { return "broken_schema"; }
  std::string_view description() const override { return "Throws on schema."; }
  Json::Value input_schema() const override {
    throw std::runtime_error("schema failure");
  }
  tool_result call(const Json::Value&) override { return tool_result::ok(""); }
};

class fake_resource_handler : public resource_handler {
public:
  using callback_type = std::function<std::string()>;

  fake_resource_handler(
      std::string uri,
      std::string name,
      std::string description,
      std::string mime_type,
      callback_type callback)
      : _uri(std::move(uri)), _name(std::move(name)),
        _description(std::move(description)), _mime_type(std::move(mime_type)),
        _callback(std::move(callback)) {}

  std::string_view uri() const override { return _uri; }
  std::string_view name() const override { return _name; }
  std::string_view description() const override { return _description; }
  std::string_view mime_type() const override { return _mime_type; }
  std::string read() override { return _callback(); }

private:
  std::string _uri;
  std::string _name;
  std::string _description;
  std::string _mime_type;
  callback_type _callback;
};

class throwing_description_resource_handler : public resource_handler {
public:
  std::string_view uri() const override { return "howling://broken-list"; }
  std::string_view name() const override { return "BrokenList"; }
  std::string_view description() const override {
    throw std::runtime_error("description failure");
  }
  std::string_view mime_type() const override { return "text/plain"; }
  std::string read() override { return ""; }
};

Json::Value make_simple_schema() {
  Json::Value schema(Json::objectValue);
  schema["type"] = "object";
  return schema;
}

class McpDispatcherTest : public ::testing::Test {
protected:
  mcp_dispatcher dispatcher{
      server_info{.name = "test-server", .version = "1.2.3"}};
};

TEST_F(McpDispatcherTest, InitializeReturnsProtocolVersionAndCapabilities) {
  const dispatch_result result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");

  ASSERT_TRUE(result.has_response());
  const Json::Value& response = *result.response;
  EXPECT_EQ(response["jsonrpc"].asString(), "2.0");
  EXPECT_EQ(response["id"].asInt(), 1);
  const Json::Value& payload = response["result"];
  EXPECT_EQ(payload["protocolVersion"].asString(), "2025-03-26");
  EXPECT_EQ(payload["serverInfo"]["name"].asString(), "test-server");
  EXPECT_EQ(payload["serverInfo"]["version"].asString(), "1.2.3");
  EXPECT_TRUE(payload["capabilities"]["tools"].isObject());
  EXPECT_TRUE(payload["capabilities"]["resources"].isObject());
}

TEST_F(McpDispatcherTest, PingReturnsEmptyObject) {
  const dispatch_result result =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":"ping-1","method":"ping"})");

  ASSERT_TRUE(result.has_response());
  const Json::Value& response = *result.response;
  EXPECT_EQ(response["id"].asString(), "ping-1");
  EXPECT_TRUE(response["result"].isObject());
  EXPECT_TRUE(response["result"].empty());
}

TEST_F(McpDispatcherTest, NotificationWithoutIdReturnsNoResponse) {
  const dispatch_result initialized = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","method":"notifications/initialized"})");
  EXPECT_FALSE(initialized.has_response());

  const dispatch_result custom = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":7}})");
  EXPECT_FALSE(custom.has_response());
}

TEST_F(McpDispatcherTest, ClientResponsesReturnNoResponse) {
  const dispatch_result client_result =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":42,"result":{"ok":true}})");
  EXPECT_FALSE(client_result.has_response());

  const dispatch_result client_error = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":"req-1","error":{"code":-32000,"message":"client failed"}})");
  EXPECT_FALSE(client_error.has_response());

  // Both "result" and "error" on the same object is invalid JSON-RPC.
  const dispatch_result ambiguous = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":43,"result":{},"error":{"code":-1,"message":"bad"}})");
  ASSERT_TRUE(ambiguous.has_response());
  EXPECT_EQ(
      (*ambiguous.response)["error"]["code"].asInt(), JSONRPC_INVALID_REQUEST);
}

TEST_F(McpDispatcherTest, BatchRequestsSupportMixedNotificationsAndErrors) {
  const dispatch_result empty_batch = dispatcher.dispatch("[]");
  ASSERT_TRUE(empty_batch.has_response());
  EXPECT_TRUE(empty_batch.response->isObject());
  EXPECT_EQ(
      (*empty_batch.response)["error"]["code"].asInt(),
      JSONRPC_INVALID_REQUEST);

  const dispatch_result all_notifications = dispatcher.dispatch(
      R"([{"jsonrpc":"2.0","method":"notifications/initialized"},{"jsonrpc":"2.0","id":9,"result":{}}])");
  EXPECT_FALSE(all_notifications.has_response());

  const dispatch_result mixed_batch = dispatcher.dispatch(
      R"([{"jsonrpc":"2.0","id":1,"method":"ping"},{"jsonrpc":"2.0","method":"notifications/initialized"},{"jsonrpc":"2.0","id":2,"method":"unknown/method"}])");
  ASSERT_TRUE(mixed_batch.has_response());
  ASSERT_TRUE(mixed_batch.response->isArray());
  ASSERT_EQ(mixed_batch.response->size(), 2u);
  EXPECT_EQ((*mixed_batch.response)[0]["id"].asInt(), 1);
  EXPECT_TRUE((*mixed_batch.response)[0]["result"].isObject());
  EXPECT_EQ((*mixed_batch.response)[1]["id"].asInt(), 2);
  EXPECT_EQ(
      (*mixed_batch.response)[1]["error"]["code"].asInt(),
      JSONRPC_METHOD_NOT_FOUND);

  const dispatch_result invalid_elements = dispatcher.dispatch("[1, 2, 3]");
  ASSERT_TRUE(invalid_elements.has_response());
  ASSERT_TRUE(invalid_elements.response->isArray());
  ASSERT_EQ(invalid_elements.response->size(), 3u);
  for (Json::ArrayIndex index = 0; index < 3; ++index) {
    EXPECT_EQ(
        (*invalid_elements.response)[index]["error"]["code"].asInt(),
        JSONRPC_INVALID_REQUEST);
    EXPECT_TRUE((*invalid_elements.response)[index]["id"].isNull());
  }
}

TEST_F(
    McpDispatcherTest,
    ToolsListReturnsRegisteredMetadataAndOmitsEmptyAnnotations) {
  tool_annotations annotations;
  annotations.read_only_hint = true;
  annotations.destructive_hint = false;
  annotations.idempotent_hint = true;
  annotations.open_world_hint = false;

  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "echo",
          "Echoes input text.",
          make_simple_schema(),
          annotations,
          [](const Json::Value&) { return tool_result::ok("ok"); }));
  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "plain",
          "Tool without annotations.",
          make_simple_schema(),
          tool_annotations{},
          [](const Json::Value&) { return tool_result::ok("ok"); }));

  const dispatch_result result =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");

  ASSERT_TRUE(result.has_response());
  const Json::Value& tools = (*result.response)["result"]["tools"];
  ASSERT_EQ(tools.size(), 2u);
  EXPECT_EQ(tools[0]["name"].asString(), "echo");
  EXPECT_EQ(tools[0]["description"].asString(), "Echoes input text.");
  EXPECT_EQ(tools[0]["inputSchema"]["type"].asString(), "object");
  ASSERT_TRUE(tools[0].isMember("annotations"));
  EXPECT_TRUE(tools[0]["annotations"]["readOnlyHint"].asBool());
  EXPECT_FALSE(tools[0]["annotations"]["destructiveHint"].asBool());
  EXPECT_TRUE(tools[0]["annotations"]["idempotentHint"].asBool());
  EXPECT_FALSE(tools[0]["annotations"]["openWorldHint"].asBool());

  EXPECT_EQ(tools[1]["name"].asString(), "plain");
  EXPECT_FALSE(tools[1].isMember("annotations"));
}

TEST_F(McpDispatcherTest, ToolsListCatchesHandlerExceptions) {
  dispatcher.register_tool(std::make_unique<throwing_schema_tool_handler>());

  const dispatch_result result =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":20,"method":"tools/list"})");
  ASSERT_TRUE(result.has_response());
  EXPECT_EQ(
      (*result.response)["error"]["code"].asInt(), JSONRPC_INTERNAL_ERROR);
  EXPECT_EQ(
      (*result.response)["error"]["message"].asString(), "schema failure");
}

TEST_F(McpDispatcherTest, ToolsCallReturnsSuccessContent) {
  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "greet",
          "Greets a caller.",
          make_simple_schema(),
          tool_annotations{},
          [](const Json::Value& arguments) {
            return tool_result::ok("Hello, " + arguments["name"].asString());
          }));

  const dispatch_result result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"greet","arguments":{"name":"wolf"}}})");

  ASSERT_TRUE(result.has_response());
  const Json::Value& payload = (*result.response)["result"];
  EXPECT_FALSE(payload["isError"].asBool());
  ASSERT_EQ(payload["content"].size(), 1u);
  EXPECT_EQ(payload["content"][0]["type"].asString(), "text");
  EXPECT_EQ(payload["content"][0]["text"].asString(), "Hello, wolf");
}

TEST_F(McpDispatcherTest, ToolsCallReturnsIsErrorOnErrorResultAndExceptions) {
  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "returns_error",
          "Returns error result.",
          make_simple_schema(),
          tool_annotations{},
          [](const Json::Value&) { return tool_result::error("bad input"); }));

  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "throws_tool_error",
          "Throws tool_error.",
          make_simple_schema(),
          tool_annotations{},
          [](const Json::Value&) -> tool_result {
            throw tool_error("symbol not found");
          }));

  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "throws_invalid_argument",
          "Throws invalid_argument.",
          make_simple_schema(),
          tool_annotations{},
          [](const Json::Value&) -> tool_result {
            throw std::invalid_argument("limit must be positive");
          }));

  const dispatch_result error_result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"returns_error"}})");
  ASSERT_TRUE(error_result.has_response());
  EXPECT_TRUE((*error_result.response)["result"]["isError"].asBool());
  EXPECT_EQ(
      (*error_result.response)["result"]["content"][0]["text"].asString(),
      "bad input");

  const dispatch_result tool_error_result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"throws_tool_error"}})");
  ASSERT_TRUE(tool_error_result.has_response());
  EXPECT_TRUE((*tool_error_result.response)["result"]["isError"].asBool());
  EXPECT_EQ(
      (*tool_error_result.response)["result"]["content"][0]["text"].asString(),
      "symbol not found");

  const dispatch_result invalid_argument_result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"throws_invalid_argument"}})");
  ASSERT_TRUE(invalid_argument_result.has_response());
  EXPECT_TRUE(
      (*invalid_argument_result.response)["result"]["isError"].asBool());
  EXPECT_EQ(
      (*invalid_argument_result.response)["result"]["content"][0]["text"]
          .asString(),
      "limit must be positive");
}

TEST_F(McpDispatcherTest, ToolsCallUnknownToolReturnsInvalidParams) {
  const dispatch_result result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"missing"}})");

  ASSERT_TRUE(result.has_response());
  EXPECT_EQ(
      (*result.response)["error"]["code"].asInt(), JSONRPC_INVALID_PARAMS);
}

TEST_F(McpDispatcherTest, ToolsCallInvalidParamsReturnsInvalidParams) {
  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "echo",
          "Echoes input.",
          make_simple_schema(),
          tool_annotations{},
          [](const Json::Value&) { return tool_result::ok("ok"); }));

  const dispatch_result missing_name = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":8,"method":"tools/call","params":{}})");
  ASSERT_TRUE(missing_name.has_response());
  EXPECT_EQ(
      (*missing_name.response)["error"]["code"].asInt(),
      JSONRPC_INVALID_PARAMS);

  const dispatch_result invalid_arguments = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"echo","arguments":[1,2,3]}})");
  ASSERT_TRUE(invalid_arguments.has_response());
  EXPECT_EQ(
      (*invalid_arguments.response)["error"]["code"].asInt(),
      JSONRPC_INVALID_PARAMS);
}

TEST_F(McpDispatcherTest, ResourcesListAndReadReturnRegisteredResource) {
  dispatcher.register_resource(
      std::make_unique<fake_resource_handler>(
          "howling://symbols",
          "Symbols",
          "Available trading symbols.",
          "application/json",
          []() { return R"({"symbols":["AAPL","SPY"]})"; }));

  const dispatch_result list_result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":10,"method":"resources/list"})");
  ASSERT_TRUE(list_result.has_response());
  const Json::Value& resources = (*list_result.response)["result"]["resources"];
  ASSERT_EQ(resources.size(), 1u);
  EXPECT_EQ(resources[0]["uri"].asString(), "howling://symbols");
  EXPECT_EQ(resources[0]["name"].asString(), "Symbols");
  EXPECT_EQ(
      resources[0]["description"].asString(), "Available trading symbols.");
  EXPECT_EQ(resources[0]["mimeType"].asString(), "application/json");

  const dispatch_result read_result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":11,"method":"resources/read","params":{"uri":"howling://symbols"}})");
  ASSERT_TRUE(read_result.has_response());
  const Json::Value& contents = (*read_result.response)["result"]["contents"];
  ASSERT_EQ(contents.size(), 1u);
  EXPECT_EQ(contents[0]["uri"].asString(), "howling://symbols");
  EXPECT_EQ(contents[0]["mimeType"].asString(), "application/json");
  EXPECT_EQ(contents[0]["text"].asString(), R"({"symbols":["AAPL","SPY"]})");
}

TEST_F(McpDispatcherTest, ResourcesListCatchesHandlerExceptions) {
  dispatcher.register_resource(
      std::make_unique<throwing_description_resource_handler>());

  const dispatch_result result = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":21,"method":"resources/list"})");
  ASSERT_TRUE(result.has_response());
  EXPECT_EQ(
      (*result.response)["error"]["code"].asInt(), JSONRPC_INTERNAL_ERROR);
  EXPECT_EQ(
      (*result.response)["error"]["message"].asString(), "description failure");
}

TEST_F(McpDispatcherTest, ResourcesReadErrorsForUnknownUriAndExceptions) {
  dispatcher.register_resource(
      std::make_unique<fake_resource_handler>(
          "howling://broken",
          "Broken",
          "Always throws.",
          "text/plain",
          []() -> std::string {
            throw std::runtime_error("database unavailable");
          }));

  const dispatch_result unknown = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":12,"method":"resources/read","params":{"uri":"howling://missing"}})");
  ASSERT_TRUE(unknown.has_response());
  EXPECT_EQ(
      (*unknown.response)["error"]["code"].asInt(), MCP_RESOURCE_NOT_FOUND);
  EXPECT_EQ(
      (*unknown.response)["error"]["data"]["uri"].asString(),
      "howling://missing");

  const dispatch_result failed = dispatcher.dispatch(
      R"({"jsonrpc":"2.0","id":13,"method":"resources/read","params":{"uri":"howling://broken"}})");
  ASSERT_TRUE(failed.has_response());
  EXPECT_EQ(
      (*failed.response)["error"]["code"].asInt(), JSONRPC_INTERNAL_ERROR);
  EXPECT_EQ(
      (*failed.response)["error"]["message"].asString(),
      "database unavailable");
}

TEST_F(McpDispatcherTest, ProtocolErrorsReturnStandardJsonRpcCodes) {
  const dispatch_result parse_error = dispatcher.dispatch("{not valid json");
  ASSERT_TRUE(parse_error.has_response());
  EXPECT_EQ(
      (*parse_error.response)["error"]["code"].asInt(), JSONRPC_PARSE_ERROR);
  EXPECT_TRUE((*parse_error.response)["id"].isNull());

  const dispatch_result not_object = dispatcher.dispatch("42");
  ASSERT_TRUE(not_object.has_response());
  EXPECT_EQ(
      (*not_object.response)["error"]["code"].asInt(), JSONRPC_INVALID_REQUEST);

  const dispatch_result wrong_version =
      dispatcher.dispatch(R"({"jsonrpc":"1.0","id":1,"method":"ping"})");
  ASSERT_TRUE(wrong_version.has_response());
  EXPECT_EQ(
      (*wrong_version.response)["error"]["code"].asInt(),
      JSONRPC_INVALID_REQUEST);

  const dispatch_result missing_method =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":1})");
  ASSERT_TRUE(missing_method.has_response());
  EXPECT_EQ(
      (*missing_method.response)["error"]["code"].asInt(),
      JSONRPC_INVALID_REQUEST);

  const dispatch_result invalid_id =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":{},"method":"ping"})");
  ASSERT_TRUE(invalid_id.has_response());
  EXPECT_EQ(
      (*invalid_id.response)["error"]["code"].asInt(), JSONRPC_INVALID_REQUEST);

  const dispatch_result bool_id =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":true,"method":"ping"})");
  ASSERT_TRUE(bool_id.has_response());
  EXPECT_EQ(
      (*bool_id.response)["error"]["code"].asInt(), JSONRPC_INVALID_REQUEST);

  const dispatch_result unknown_method =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","id":99,"method":"foo/bar"})");
  ASSERT_TRUE(unknown_method.has_response());
  EXPECT_EQ(
      (*unknown_method.response)["error"]["code"].asInt(),
      JSONRPC_METHOD_NOT_FOUND);
  EXPECT_EQ((*unknown_method.response)["id"].asInt(), 99);
}

TEST_F(McpDispatcherTest, RegistrationRejectsDuplicatesAndInvalidInputs) {
  EXPECT_THROW(
      mcp_dispatcher(server_info{.name = "", .version = "1.0"}),
      std::invalid_argument);
  EXPECT_THROW(
      mcp_dispatcher(server_info{.name = "server", .version = ""}),
      std::invalid_argument);

  EXPECT_THROW(dispatcher.register_tool(nullptr), std::invalid_argument);
  EXPECT_THROW(dispatcher.register_resource(nullptr), std::invalid_argument);

  EXPECT_THROW(
      dispatcher.register_tool(
          std::make_unique<fake_tool_handler>(
              "",
              "Missing name.",
              make_simple_schema(),
              tool_annotations{},
              [](const Json::Value&) { return tool_result::ok("ok"); })),
      std::invalid_argument);

  EXPECT_THROW(
      dispatcher.register_resource(
          std::make_unique<fake_resource_handler>(
              "",
              "MissingUri",
              "Missing URI.",
              "text/plain",
              []() { return ""; })),
      std::invalid_argument);

  EXPECT_THROW(
      dispatcher.register_resource(
          std::make_unique<fake_resource_handler>(
              "howling://missing-name",
              "",
              "Missing name.",
              "text/plain",
              []() { return ""; })),
      std::invalid_argument);

  dispatcher.register_tool(
      std::make_unique<fake_tool_handler>(
          "duplicate_tool",
          "First.",
          make_simple_schema(),
          tool_annotations{},
          [](const Json::Value&) { return tool_result::ok("ok"); }));
  EXPECT_THROW(
      dispatcher.register_tool(
          std::make_unique<fake_tool_handler>(
              "duplicate_tool",
              "Second.",
              make_simple_schema(),
              tool_annotations{},
              [](const Json::Value&) { return tool_result::ok("ok"); })),
      std::invalid_argument);

  dispatcher.register_resource(
      std::make_unique<fake_resource_handler>(
          "howling://duplicate", "First", "First.", "text/plain", []() {
            return "a";
          }));
  EXPECT_THROW(
      dispatcher.register_resource(
          std::make_unique<fake_resource_handler>(
              "howling://duplicate",
              "Second",
              "Second.",
              "text/plain",
              []() { return "b"; })),
      std::invalid_argument);
}

} // namespace
} // namespace howling::mcp
