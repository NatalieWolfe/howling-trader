#include "services/reader_mcp/service.h"

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "data/stock.pb.h"
#include "services/db/reader/mock_reader_db.h"
#include "services/db/reader/reader_db.h"
#include "services/mcp/dispatcher.h"
#include "strings/json.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json/value.h"

namespace howling::reader_mcp {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrictMock;

class ReaderMcpServiceTest : public ::testing::Test {
protected:
  ReaderMcpServiceTest()
      : _pool(
            [this]() -> std::unique_ptr<reader_db> {
              auto mock = std::make_unique<StrictMock<mock_reader_db>>();
              EXPECT_CALL(*mock, is_healthy())
                  .Times(AnyNumber())
                  .WillRepeatedly(Return(true));
              EXPECT_CALL(*mock, reset()).Times(AnyNumber());
              _mock_db = mock.get();
              return mock;
            },
            reader_db_pool::options{.min_size = 1, .max_size = 1}),
        _service(_pool) {}

  StrictMock<mock_reader_db>* _mock_db = nullptr;
  reader_db_pool _pool;
  reader_mcp_service _service;
};

TEST_F(ReaderMcpServiceTest, InitializeAndListsExposeAllToolsAndResources) {
  const mcp::dispatch_result initialize_result = _service.dispatch(
      R"json({"jsonrpc":"2.0","id":1,"method":"initialize"})json");
  ASSERT_TRUE(initialize_result.has_response());
  const Json::Value& initialize_payload =
      (*initialize_result.response)["result"];
  EXPECT_EQ(initialize_payload["protocolVersion"].asString(), "2025-03-26");
  EXPECT_EQ(
      initialize_payload["serverInfo"]["name"].asString(),
      "howling-reader-mcp");
  EXPECT_EQ(initialize_payload["serverInfo"]["version"].asString(), "1.0.0");

  const mcp::dispatch_result tools_list = _service.dispatch(
      R"json({"jsonrpc":"2.0","id":2,"method":"tools/list"})json");
  ASSERT_TRUE(tools_list.has_response());
  const Json::Value& tools = (*tools_list.response)["result"]["tools"];
  ASSERT_EQ(tools.size(), 4U);
  std::set<std::string> tool_names;
  for (const Json::Value& tool : tools) {
    tool_names.insert(tool["name"].asString());
    EXPECT_TRUE(tool["annotations"]["readOnlyHint"].asBool());
  }
  EXPECT_EQ(
      tool_names,
      (std::set<std::string>{
          "get_candles",
          "get_market_data",
          "get_trades",
          "get_trading_summary"}));

  const mcp::dispatch_result resources_list = _service.dispatch(
      R"json({"jsonrpc":"2.0","id":3,"method":"resources/list"})json");
  ASSERT_TRUE(resources_list.has_response());
  const Json::Value& resources =
      (*resources_list.response)["result"]["resources"];
  ASSERT_EQ(resources.size(), 3U);
  std::set<std::string> resource_uris;
  for (const Json::Value& resource : resources) {
    resource_uris.insert(resource["uri"].asString());
    EXPECT_EQ(resource["mimeType"].asString(), "application/json");
  }
  EXPECT_EQ(
      resource_uris,
      (std::set<std::string>{
          "howling://symbols", "howling://actions", "howling://schema"}));
}

TEST_F(ReaderMcpServiceTest, ToolsCallDispatchesAndHandlesValidationErrors) {
  candle_page page;
  page.has_more = false;
  page.rows.push_back(
      candle_row{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .opened_at = std::chrono::sys_days{std::chrono::year{2026} / 3 / 15} +
              std::chrono::hours{14},
          .open = 130.0,
          .close = 131.0,
          .high = 132.0,
          .low = 129.5,
          .volume = 1000,
          .duration_us = 60000000});

  EXPECT_CALL(*_mock_db, read_candles(_)).WillOnce(Return(page));

  const mcp::dispatch_result ok_result = _service.dispatch(R"json({
    "jsonrpc": "2.0",
    "id": 10,
    "method": "tools/call",
    "params": {
      "name": "get_candles",
      "arguments": {"symbol": "NVDA", "limit": 10}
    }
  })json");
  ASSERT_TRUE(ok_result.has_response());
  EXPECT_FALSE((*ok_result.response)["result"]["isError"].asBool());
  const Json::Value body =
      to_json((*ok_result.response)["result"]["content"][0]["text"].asString());
  EXPECT_EQ(body["symbol"].asString(), "NVDA");
  EXPECT_EQ(body["count"].asInt64(), 1);
  EXPECT_FALSE(body["has_more"].asBool());

  const mcp::dispatch_result error_result = _service.dispatch(R"json({
    "jsonrpc": "2.0",
    "id": 11,
    "method": "tools/call",
    "params": {
      "name": "get_candles",
      "arguments": {"symbol": "INVALID", "limit": 10}
    }
  })json");
  ASSERT_TRUE(error_result.has_response());
  EXPECT_TRUE((*error_result.response)["result"]["isError"].asBool());
}

TEST_F(ReaderMcpServiceTest, ResourcesReadDispatchesToHandlers) {
  const std::vector<action_info> actions = {
      {.id = 1, .name = "HOLD"},
      {.id = 2, .name = "BUY"},
      {.id = 3, .name = "SELL"}};
  EXPECT_CALL(*_mock_db, read_actions_metadata()).WillOnce(Return(actions));

  const mcp::dispatch_result result = _service.dispatch(R"json({
    "jsonrpc": "2.0",
    "id": 20,
    "method": "resources/read",
    "params": {"uri": "howling://actions"}
  })json");
  ASSERT_TRUE(result.has_response());
  ASSERT_EQ((*result.response)["result"]["contents"].size(), 1U);
  const Json::Value& content = (*result.response)["result"]["contents"][0];
  EXPECT_EQ(content["uri"].asString(), "howling://actions");
  EXPECT_EQ(content["mimeType"].asString(), "application/json");
  const Json::Value body = to_json(content["text"].asString());
  ASSERT_EQ(body["actions"].size(), 3U);
  EXPECT_EQ(body["actions"][2]["name"].asString(), "SELL");
}

} // namespace
} // namespace howling::reader_mcp
