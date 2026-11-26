#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "services/db/reader/mock_reader_db.h"
#include "services/db/reader/reader_db.h"
#include "services/mcp/tool_handler.h"
#include "services/reader_mcp/handlers/actions_resource_handler.h"
#include "services/reader_mcp/handlers/candles_handler.h"
#include "services/reader_mcp/handlers/market_data_handler.h"
#include "services/reader_mcp/handlers/schema_resource_handler.h"
#include "services/reader_mcp/handlers/symbols_resource_handler.h"
#include "services/reader_mcp/handlers/trades_handler.h"
#include "services/reader_mcp/handlers/trading_summary_handler.h"
#include "services/reader_mcp/handlers/validation.h"
#include "strings/json.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json/value.h"

namespace howling::reader_mcp {
namespace {

using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Return;
using ::testing::StrictMock;

std::chrono::system_clock::time_point make_utc(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second,
    int microseconds = 0) {
  const std::chrono::sys_days date = std::chrono::year{year} /
      std::chrono::month{static_cast<unsigned>(month)} /
      std::chrono::day{static_cast<unsigned>(day)};
  return date + std::chrono::hours{hour} + std::chrono::minutes{minute} +
      std::chrono::seconds{second} + std::chrono::microseconds{microseconds};
}

class ReaderMcpHandlersTest : public ::testing::Test {
protected:
  ReaderMcpHandlersTest()
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
            reader_db_pool::options{.min_size = 1, .max_size = 1}) {}

  StrictMock<mock_reader_db>* _mock_db = nullptr;
  reader_db_pool _pool;
};

TEST(ValidationTest, ParsesAndNormalizesUtcOffsets) {
  const std::chrono::system_clock::time_point expected =
      make_utc(2026, 3, 15, 14, 30, 0);

  EXPECT_EQ(parse_iso8601_utc("2026-03-15T14:30:00Z"), expected);
  EXPECT_EQ(parse_iso8601_utc("2026-03-15T10:30:00-04:00"), expected);
  EXPECT_EQ(parse_iso8601_utc("2026-03-15T16:30:00+02:00"), expected);

  const std::chrono::system_clock::time_point expected_fractional =
      make_utc(2026, 3, 15, 14, 30, 0, 123456);
  EXPECT_EQ(
      parse_iso8601_utc("2026-03-15T10:30:00.123456-04:00"),
      expected_fractional);
  EXPECT_EQ(format_utc_timestamp(expected), "2026-03-15T14:30:00Z");
  EXPECT_EQ(
      format_utc_timestamp(expected_fractional), "2026-03-15T14:30:00.123456Z");
}

TEST(ValidationTest, RejectsInvalidAndOutOfRangeTimestamps) {
  EXPECT_THROW((void)parse_iso8601_utc(""), mcp::tool_error);
  EXPECT_THROW((void)parse_iso8601_utc("not-a-timestamp"), mcp::tool_error);
  EXPECT_THROW((void)parse_iso8601_utc("2026-03-15T14:30:00"), mcp::tool_error);
  EXPECT_THROW(
      (void)parse_iso8601_utc("2026-02-30T00:00:00Z"), mcp::tool_error);
  EXPECT_THROW(
      (void)parse_iso8601_utc("9999-12-31T23:59:59Z"), mcp::tool_error);
  EXPECT_THROW(
      (void)parse_iso8601_utc("1969-12-31T23:59:59Z"), mcp::tool_error);
  EXPECT_THROW(
      (void)parse_iso8601_utc("0001-01-01T00:00:00Z"), mcp::tool_error);
}

TEST(ValidationTest, EnumParsingAndNameFallback) {
  EXPECT_EQ(parse_symbol("NVDA"), static_cast<int32_t>(stock::NVDA));
  EXPECT_THROW((void)parse_symbol("SYMBOL_UNSPECIFIED"), mcp::tool_error);
  EXPECT_THROW((void)parse_symbol("nvda"), mcp::tool_error);
  EXPECT_EQ(symbol_name_from_id(static_cast<int32_t>(stock::NVDA)), "NVDA");
  EXPECT_EQ(
      symbol_name_from_id(static_cast<int32_t>(stock::SYMBOL_UNSPECIFIED)),
      "UNKNOWN");
  EXPECT_EQ(symbol_name_from_id(999999), "UNKNOWN");

  EXPECT_EQ(parse_action("BUY"), static_cast<int32_t>(trading::BUY));
  EXPECT_THROW((void)parse_action("ACTION_UNSPECIFIED"), mcp::tool_error);
  EXPECT_THROW((void)parse_action("buy"), mcp::tool_error);
  EXPECT_EQ(action_name_from_id(static_cast<int32_t>(trading::SELL)), "SELL");
  EXPECT_EQ(
      action_name_from_id(static_cast<int32_t>(trading::ACTION_UNSPECIFIED)),
      "UNKNOWN");
  EXPECT_EQ(action_name_from_id(999999), "UNKNOWN");
}

TEST(ValidationTest, RejectsLargeUnsignedIntegersAndInvalidNumericTypes) {
  const Json::Value too_large_uint64{
      static_cast<Json::UInt64>(std::numeric_limits<int64_t>::max()) + 1};

  {
    Json::Value arguments(Json::objectValue);
    arguments["bucket_seconds"] = too_large_uint64;
    EXPECT_THROW(
        (void)parse_optional_positive_int64(arguments, "bucket_seconds"),
        mcp::tool_error);
  }
  {
    Json::Value arguments(Json::objectValue);
    arguments["limit"] = too_large_uint64;
    EXPECT_THROW((void)parse_pagination(arguments), mcp::tool_error);
  }
  {
    Json::Value arguments(Json::objectValue);
    arguments["offset"] = too_large_uint64;
    EXPECT_THROW((void)parse_pagination(arguments), mcp::tool_error);
  }
  {
    Json::Value arguments(Json::objectValue);
    arguments["limit"] = 10.5;
    EXPECT_THROW((void)parse_pagination(arguments), mcp::tool_error);
  }
  {
    Json::Value arguments(Json::objectValue);
    arguments["offset"] = Json::Int64{
        static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1};
    EXPECT_THROW((void)parse_pagination(arguments), mcp::tool_error);
  }
}

TEST(ValidationTest, OptionalFieldsTreatJsonNullAsUnset) {
  Json::Value arguments(Json::objectValue);
  arguments["symbol"] = Json::Value(Json::nullValue);
  arguments["dry_run"] = Json::Value(Json::nullValue);
  arguments["bucket_seconds"] = Json::Value(Json::nullValue);
  arguments["start_time"] = Json::Value(Json::nullValue);
  arguments["end_time"] = Json::Value(Json::nullValue);
  arguments["limit"] = Json::Value(Json::nullValue);
  arguments["offset"] = Json::Value(Json::nullValue);
  arguments["order"] = Json::Value(Json::nullValue);

  EXPECT_EQ(parse_optional_string(arguments, "symbol"), std::nullopt);
  EXPECT_EQ(parse_optional_bool(arguments, "dry_run"), std::nullopt);
  EXPECT_EQ(
      parse_optional_positive_int64(arguments, "bucket_seconds"), std::nullopt);
  const time_range range = parse_time_range(arguments);
  EXPECT_EQ(range.start_time, std::nullopt);
  EXPECT_EQ(range.end_time, std::nullopt);
  const pagination_params pagination = parse_pagination(arguments);
  EXPECT_EQ(pagination.limit, DEFAULT_LIMIT);
  EXPECT_EQ(pagination.offset, 0);
  EXPECT_EQ(parse_sort_order(arguments), sort_order::DESC);
}

TEST_F(ReaderMcpHandlersTest, ToolSchemasSetStrictMetadata) {
  candles_handler candles(_pool);
  market_data_handler market_data(_pool);
  trades_handler trades(_pool);
  trading_summary_handler summary(_pool);

  const Json::Value candles_schema = candles.input_schema();
  EXPECT_FALSE(candles_schema["additionalProperties"].asBool());
  EXPECT_EQ(
      candles_schema["properties"]["start_time"]["format"].asString(),
      "date-time");
  EXPECT_EQ(
      candles_schema["properties"]["end_time"]["format"].asString(),
      "date-time");
  EXPECT_EQ(
      candles_schema["properties"]["bucket_seconds"]["maximum"].asInt64(),
      MAX_BUCKET_SECONDS);

  EXPECT_FALSE(market_data.input_schema()["additionalProperties"].asBool());
  EXPECT_FALSE(trades.input_schema()["additionalProperties"].asBool());
  EXPECT_FALSE(summary.input_schema()["additionalProperties"].asBool());
}

TEST_F(ReaderMcpHandlersTest, CandlesHandlerHappyPathAndBucket) {
  candles_handler handler(_pool);
  EXPECT_EQ(handler.name(), "get_candles");
  EXPECT_EQ(handler.annotations().read_only_hint, true);

  const std::chrono::system_clock::time_point start =
      make_utc(2026, 3, 15, 14, 0, 0);
  const std::chrono::system_clock::time_point end =
      make_utc(2026, 3, 15, 15, 0, 0);

  candle_page page;
  page.has_more = true;
  page.rows.push_back(
      candle_row{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .opened_at = start,
          .open = 120.5,
          .close = 122.0,
          .high = 123.0,
          .low = 119.5,
          .volume = 50000,
          .duration_us = 300000000});

  EXPECT_CALL(
      *_mock_db,
      read_candles(AllOf(
          Field(
              &candle_query::symbol_id, Eq(static_cast<int32_t>(stock::NVDA))),
          Field(&candle_query::start_time, Eq(std::optional{start})),
          Field(&candle_query::end_time, Eq(std::optional{end})),
          Field(&candle_query::order, Eq(sort_order::ASC)),
          Field(&candle_query::limit, Eq(50)),
          Field(&candle_query::offset, Eq(10)),
          Field(
              &candle_query::bucket_seconds, Eq(std::optional<int64_t>{300})))))
      .WillOnce(Return(page));

  Json::Value arguments(Json::objectValue);
  arguments["symbol"] = "NVDA";
  arguments["start_time"] = "2026-03-15T10:00:00-04:00";
  arguments["end_time"] = "2026-03-15T15:00:00Z";
  arguments["limit"] = 50;
  arguments["offset"] = 10;
  arguments["order"] = "asc";
  arguments["bucket_seconds"] = 300;

  const mcp::tool_result result = handler.call(arguments);
  EXPECT_FALSE(result.is_error);

  const Json::Value json = to_json(result.text);
  EXPECT_EQ(json["symbol"].asString(), "NVDA");
  EXPECT_EQ(json["bucket_seconds"].asInt64(), 300);
  EXPECT_EQ(json["count"].asInt64(), 1);
  EXPECT_EQ(json["limit"].asInt(), 50);
  EXPECT_EQ(json["offset"].asInt(), 10);
  EXPECT_TRUE(json["has_more"].asBool());
  ASSERT_EQ(json["candles"].size(), 1U);
  EXPECT_EQ(json["candles"][0]["opened_at"].asString(), "2026-03-15T14:00:00Z");
  EXPECT_DOUBLE_EQ(json["candles"][0]["open"].asDouble(), 120.5);
  EXPECT_EQ(json["candles"][0]["volume"].asInt64(), 50000);
}

TEST_F(ReaderMcpHandlersTest, CandlesHandlerRejectsInvalidInputs) {
  candles_handler handler(_pool);

  // Reject non-object tool arguments.
  EXPECT_THROW(
      (void)handler.call(Json::Value(Json::arrayValue)), mcp::tool_error);

  // Reject missing required symbol argument.
  EXPECT_THROW(
      (void)handler.call(Json::Value(Json::objectValue)), mcp::tool_error);

  // Reject unknown argument keys so mistyped filters are never ignored.
  {
    Json::Value arguments(Json::objectValue);
    arguments["symbol"] = "NVDA";
    arguments["bucket"] = 60;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);
  }

  // Reject unknown or non-canonical symbol names.
  {
    Json::Value arguments(Json::objectValue);
    arguments["symbol"] = "DOGE";
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["symbol"] = "SYMBOL_UNSPECIFIED";
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["symbol"] = "nvda";
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);
  }

  // Reject non-positive or excessively large bucket_seconds values.
  {
    Json::Value arguments(Json::objectValue);
    arguments["symbol"] = "NVDA";
    arguments["bucket_seconds"] = 0;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["bucket_seconds"] = -60;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["bucket_seconds"] = Json::Int64{MAX_BUCKET_SECONDS + 1};
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["bucket_seconds"] = Json::Value{
        static_cast<Json::UInt64>(std::numeric_limits<int64_t>::max()) + 1};
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);
  }

  // Reject out-of-range or non-integer limit and offset values.
  {
    Json::Value arguments(Json::objectValue);
    arguments["symbol"] = "NVDA";
    arguments["limit"] = 0;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["limit"] = MAX_LIMIT + 1;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["limit"] = true;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["limit"] = 10.5;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["limit"] = 10;
    arguments["offset"] = -1;
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);

    arguments["offset"] = Json::Int64{
        static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1};
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);
  }

  // Reject unrecognized sort order values.
  {
    Json::Value arguments(Json::objectValue);
    arguments["symbol"] = "NVDA";
    arguments["order"] = "sideways";
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);
  }

  // Reject inverted time ranges where start_time exceeds end_time.
  {
    Json::Value arguments(Json::objectValue);
    arguments["symbol"] = "NVDA";
    arguments["start_time"] = "2026-03-15T15:00:00Z";
    arguments["end_time"] = "2026-03-15T14:00:00Z";
    EXPECT_THROW((void)handler.call(arguments), mcp::tool_error);
  }
}

TEST_F(ReaderMcpHandlersTest, MarketDataHandlerHappyPathAndValidation) {
  market_data_handler handler(_pool);
  EXPECT_EQ(handler.name(), "get_market_data");
  EXPECT_EQ(handler.annotations().read_only_hint, true);

  const std::chrono::system_clock::time_point tick_time =
      make_utc(2026, 3, 15, 14, 30, 1, 500000);

  market_page page;
  page.has_more = false;
  page.rows.push_back(
      market_row{
          .symbol_id = static_cast<int32_t>(stock::AAPL),
          .emitted_at = tick_time,
          .bid = 210.10,
          .bid_lots = 5,
          .ask = 210.20,
          .ask_lots = 8,
          .last = 210.15,
          .last_lots = 2});

  EXPECT_CALL(
      *_mock_db,
      read_market_data(AllOf(
          Field(
              &market_query::symbol_id, Eq(static_cast<int32_t>(stock::AAPL))),
          Field(&market_query::limit, Eq(DEFAULT_LIMIT)),
          Field(&market_query::offset, Eq(0)),
          Field(&market_query::order, Eq(sort_order::DESC)))))
      .WillOnce(Return(page));

  Json::Value arguments(Json::objectValue);
  arguments["symbol"] = "AAPL";
  arguments["start_time"] = Json::Value(Json::nullValue);
  arguments["end_time"] = Json::Value(Json::nullValue);

  const mcp::tool_result result = handler.call(arguments);
  EXPECT_FALSE(result.is_error);

  const Json::Value json = to_json(result.text);
  EXPECT_EQ(json["symbol"].asString(), "AAPL");
  EXPECT_FALSE(json["has_more"].asBool());
  ASSERT_EQ(json["quotes"].size(), 1U);
  EXPECT_EQ(
      json["quotes"][0]["emitted_at"].asString(), "2026-03-15T14:30:01.5Z");
  EXPECT_DOUBLE_EQ(json["quotes"][0]["bid"].asDouble(), 210.10);
  EXPECT_EQ(json["quotes"][0]["ask_lots"].asInt64(), 8);

  // Reject non-object arguments and unknown keys.
  EXPECT_THROW((void)handler.call(Json::Value("AAPL")), mcp::tool_error);
  Json::Value extra_arguments(Json::objectValue);
  extra_arguments["symbol"] = "AAPL";
  extra_arguments["unknown_key"] = true;
  EXPECT_THROW((void)handler.call(extra_arguments), mcp::tool_error);
}

TEST_F(ReaderMcpHandlersTest, TradesHandlerFiltersFallbacksAndValidation) {
  trades_handler handler(_pool);
  EXPECT_EQ(handler.name(), "get_trades");
  EXPECT_EQ(handler.annotations().read_only_hint, true);

  const std::chrono::system_clock::time_point executed_at =
      make_utc(2026, 3, 15, 14, 45, 0);

  trade_page page;
  page.has_more = true;
  page.rows.push_back(
      trade_row{
          .executed_at = executed_at,
          .symbol_id = static_cast<int32_t>(stock::AMD),
          .symbol_name = "",
          .action_id = static_cast<int32_t>(trading::BUY),
          .action_name = "",
          .price = 155.25,
          .quantity = 10,
          .confidence = 0.91,
          .dry_run = false});

  EXPECT_CALL(
      *_mock_db,
      read_trades(AllOf(
          Field(
              &trade_query::symbol_id,
              Eq(std::optional<int32_t>{static_cast<int32_t>(stock::AMD)})),
          Field(
              &trade_query::action_id,
              Eq(std::optional<int32_t>{static_cast<int32_t>(trading::BUY)})),
          Field(&trade_query::dry_run, Eq(std::optional<bool>{false})),
          Field(&trade_query::limit, Eq(25)),
          Field(&trade_query::offset, Eq(5)))))
      .WillOnce(Return(page));

  Json::Value arguments(Json::objectValue);
  arguments["symbol"] = "AMD";
  arguments["action"] = "BUY";
  arguments["dry_run"] = false;
  arguments["limit"] = 25;
  arguments["offset"] = 5;

  const mcp::tool_result result = handler.call(arguments);
  EXPECT_FALSE(result.is_error);

  const Json::Value json = to_json(result.text);
  EXPECT_TRUE(json["has_more"].asBool());
  ASSERT_EQ(json["trades"].size(), 1U);
  EXPECT_EQ(json["trades"][0]["symbol"].asString(), "AMD");
  EXPECT_EQ(json["trades"][0]["action"].asString(), "BUY");
  EXPECT_EQ(
      json["trades"][0]["executed_at"].asString(), "2026-03-15T14:45:00Z");
  EXPECT_FALSE(json["trades"][0]["dry_run"].asBool());

  // Reject non-object arguments, unknown keys, non-boolean dry_run, and invalid
  // action enum names.
  EXPECT_THROW((void)handler.call(Json::Value(42)), mcp::tool_error);

  Json::Value bad_action(Json::objectValue);
  bad_action["action"] = "SHORT";
  EXPECT_THROW((void)handler.call(bad_action), mcp::tool_error);

  bad_action["action"] = "ACTION_UNSPECIFIED";
  EXPECT_THROW((void)handler.call(bad_action), mcp::tool_error);

  bad_action["action"] = "buy";
  EXPECT_THROW((void)handler.call(bad_action), mcp::tool_error);

  Json::Value bad_dry_run(Json::objectValue);
  bad_dry_run["dry_run"] = "false";
  EXPECT_THROW((void)handler.call(bad_dry_run), mcp::tool_error);

  Json::Value unknown_key(Json::objectValue);
  unknown_key["dryRun"] = true;
  EXPECT_THROW((void)handler.call(unknown_key), mcp::tool_error);
}

TEST_F(ReaderMcpHandlersTest, TradingSummaryRejectsEmptyClockCallback) {
  EXPECT_THROW(
      (void)trading_summary_handler(_pool, nullptr), std::invalid_argument);
}

TEST_F(
    ReaderMcpHandlersTest,
    TradingSummaryDefaultsToLast24HoursAndIncludesZeroTradeSymbols) {
  const std::chrono::system_clock::time_point fixed_now =
      make_utc(2026, 3, 15, 20, 0, 0);
  const std::chrono::system_clock::time_point expected_start =
      fixed_now - std::chrono::hours(24);

  trading_summary_handler handler(_pool, [fixed_now]() { return fixed_now; });
  EXPECT_EQ(handler.name(), "get_trading_summary");
  EXPECT_EQ(handler.annotations().read_only_hint, true);

  std::vector<trading_summary_row> rows;
  rows.push_back(
      trading_summary_row{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .symbol_name = "NVDA",
          .total_trades = 2,
          .buy_count = 1,
          .sell_count = 1,
          .hold_count = 0,
          .buy_quantity = 10,
          .sell_quantity = 10,
          .buy_notional = 1200.0,
          .sell_notional = 1250.0,
          .dry_run_count = 0,
          .live_count = 2,
          .avg_confidence = 0.85,
          .first_trade_at = make_utc(2026, 3, 15, 14, 0, 0),
          .last_trade_at = make_utc(2026, 3, 15, 16, 0, 0),
          .candle_count = 390,
          .open = 119.0,
          .close = 125.0,
          .high = 126.0,
          .low = 118.5,
          .total_volume = 1000000});
  // Include a symbol with candles and zero trades to verify null trade fields.
  rows.push_back(
      trading_summary_row{
          .symbol_id = static_cast<int32_t>(stock::MU),
          .symbol_name = "MU",
          .total_trades = 0,
          .candle_count = 390,
          .open = 95.0,
          .close = 96.5,
          .high = 97.0,
          .low = 94.5,
          .total_volume = 250000});

  EXPECT_CALL(
      *_mock_db,
      read_trading_summary(AllOf(
          Field(&summary_query::symbol_id, Eq(std::nullopt)),
          Field(&summary_query::dry_run, Eq(std::nullopt)),
          Field(&summary_query::start_time, Eq(std::optional{expected_start})),
          Field(&summary_query::end_time, Eq(std::optional{fixed_now})))))
      .WillOnce(Return(rows));

  const mcp::tool_result result = handler.call(Json::Value(Json::objectValue));
  EXPECT_FALSE(result.is_error);

  const Json::Value json = to_json(result.text);
  EXPECT_EQ(json["start_time"].asString(), "2026-03-14T20:00:00Z");
  EXPECT_EQ(json["end_time"].asString(), "2026-03-15T20:00:00Z");
  ASSERT_EQ(json["summaries"].size(), 2U);

  EXPECT_EQ(json["summaries"][0]["symbol"].asString(), "NVDA");
  EXPECT_DOUBLE_EQ(json["summaries"][0]["buy_notional"].asDouble(), 1200.0);
  EXPECT_DOUBLE_EQ(json["summaries"][0]["sell_notional"].asDouble(), 1250.0);

  EXPECT_EQ(json["summaries"][1]["symbol"].asString(), "MU");
  EXPECT_EQ(json["summaries"][1]["total_trades"].asInt64(), 0);
  EXPECT_TRUE(json["summaries"][1]["avg_confidence"].isNull());
  EXPECT_TRUE(json["summaries"][1]["first_trade_at"].isNull());
  EXPECT_EQ(json["summaries"][1]["candle_count"].asInt64(), 390);
}

TEST_F(
    ReaderMcpHandlersTest, TradingSummarySingleBoundsFiltersAndInvalidInputs) {
  trading_summary_handler handler(_pool);
  const std::chrono::system_clock::time_point start =
      make_utc(2026, 3, 15, 13, 30, 0);
  const std::chrono::system_clock::time_point end =
      make_utc(2026, 3, 15, 20, 0, 0);

  EXPECT_CALL(
      *_mock_db,
      read_trading_summary(AllOf(
          Field(&summary_query::symbol_id, Eq(std::nullopt)),
          Field(&summary_query::dry_run, Eq(std::nullopt)),
          Field(&summary_query::start_time, Eq(std::optional{start})),
          Field(&summary_query::end_time, Eq(std::nullopt)))))
      .WillOnce(Return(std::vector<trading_summary_row>{}));

  Json::Value start_only_arguments(Json::objectValue);
  start_only_arguments["start_time"] = "2026-03-15T13:30:00Z";
  const mcp::tool_result start_result = handler.call(start_only_arguments);
  EXPECT_FALSE(start_result.is_error);
  const Json::Value start_json = to_json(start_result.text);
  EXPECT_EQ(start_json["start_time"].asString(), "2026-03-15T13:30:00Z");
  EXPECT_TRUE(start_json["end_time"].isNull());

  EXPECT_CALL(
      *_mock_db,
      read_trading_summary(AllOf(
          Field(
              &summary_query::symbol_id,
              Eq(std::optional<int32_t>{static_cast<int32_t>(stock::NVDA)})),
          Field(&summary_query::dry_run, Eq(std::optional<bool>{true})),
          Field(&summary_query::start_time, Eq(std::nullopt)),
          Field(&summary_query::end_time, Eq(std::optional{end})))))
      .WillOnce(Return(std::vector<trading_summary_row>{}));

  Json::Value end_only_arguments(Json::objectValue);
  end_only_arguments["symbol"] = "NVDA";
  end_only_arguments["dry_run"] = true;
  end_only_arguments["end_time"] = "2026-03-15T20:00:00Z";
  const mcp::tool_result end_result = handler.call(end_only_arguments);
  EXPECT_FALSE(end_result.is_error);
  const Json::Value end_json = to_json(end_result.text);
  EXPECT_TRUE(end_json["start_time"].isNull());
  EXPECT_EQ(end_json["end_time"].asString(), "2026-03-15T20:00:00Z");

  // Reject inverted ranges, non-object arguments, non-boolean dry_run, and
  // unknown keys.
  Json::Value bad_range(Json::objectValue);
  bad_range["start_time"] = "2026-03-15T16:00:00Z";
  bad_range["end_time"] = "2026-03-15T15:00:00Z";
  EXPECT_THROW((void)handler.call(bad_range), mcp::tool_error);

  EXPECT_THROW(
      (void)handler.call(Json::Value(Json::nullValue)), mcp::tool_error);

  Json::Value bad_dry_run(Json::objectValue);
  bad_dry_run["dry_run"] = 1;
  EXPECT_THROW((void)handler.call(bad_dry_run), mcp::tool_error);

  Json::Value unknown_key(Json::objectValue);
  unknown_key["start"] = "2026-03-15T13:30:00Z";
  EXPECT_THROW((void)handler.call(unknown_key), mcp::tool_error);
}

TEST_F(ReaderMcpHandlersTest, SymbolsResourceHandlerFormatsMetadata) {
  symbols_resource_handler handler(_pool);
  EXPECT_EQ(handler.uri(), "howling://symbols");
  EXPECT_EQ(handler.mime_type(), "application/json");

  symbols_metadata metadata;
  metadata.symbols.push_back(
      symbol_metadata{
          .symbol_id = 1,
          .symbol_name = "NVDA",
          .min_candle_at = make_utc(2026, 3, 1, 14, 30, 0),
          .max_candle_at = make_utc(2026, 3, 15, 20, 0, 0),
          .trade_count = 42});
  metadata.table_estimates.push_back(
      table_row_estimate{.table_name = "candles", .approx_rows = 123456});

  EXPECT_CALL(*_mock_db, read_symbols_metadata()).WillOnce(Return(metadata));

  const Json::Value json = to_json(handler.read());
  ASSERT_EQ(json["symbols"].size(), 1U);
  EXPECT_EQ(json["symbols"][0]["symbol"].asString(), "NVDA");
  EXPECT_EQ(
      json["symbols"][0]["min_candle_at"].asString(), "2026-03-01T14:30:00Z");
  EXPECT_TRUE(json["symbols"][0]["min_market_at"].isNull());
  EXPECT_EQ(json["symbols"][0]["trade_count"].asInt64(), 42);
  ASSERT_EQ(json["table_estimates"].size(), 1U);
  EXPECT_EQ(json["table_estimates"][0]["table"].asString(), "candles");
  EXPECT_EQ(json["table_estimates"][0]["approx_rows"].asInt64(), 123456);
}

TEST_F(ReaderMcpHandlersTest, ActionsResourceHandlerFormatsActions) {
  actions_resource_handler handler(_pool);
  EXPECT_EQ(handler.uri(), "howling://actions");
  EXPECT_EQ(handler.mime_type(), "application/json");

  const std::vector<action_info> actions = {
      {.id = 1, .name = "HOLD"},
      {.id = 2, .name = "BUY"},
      {.id = 3, .name = "SELL"}};
  EXPECT_CALL(*_mock_db, read_actions_metadata()).WillOnce(Return(actions));

  const Json::Value json = to_json(handler.read());
  ASSERT_EQ(json["actions"].size(), 3U);
  EXPECT_EQ(json["actions"][1]["id"].asInt(), 2);
  EXPECT_EQ(json["actions"][1]["name"].asString(), "BUY");
}

TEST_F(ReaderMcpHandlersTest, SchemaResourceHandlerFormatsSchema) {
  schema_resource_handler handler(_pool);
  EXPECT_EQ(handler.uri(), "howling://schema");
  EXPECT_EQ(handler.mime_type(), "application/json");

  schema_metadata metadata;
  metadata.schema_version = 4;
  metadata.tables.push_back(
      table_metadata{
          .table_name = "candles",
          .columns = {
              {.column_name = "symbol",
               .data_type = "integer",
               .is_nullable = false},
              {.column_name = "open",
               .data_type = "double precision",
               .is_nullable = false}}});

  EXPECT_CALL(*_mock_db, read_schema_metadata()).WillOnce(Return(metadata));

  const Json::Value json = to_json(handler.read());
  EXPECT_EQ(json["schema_version"].asInt(), 4);
  ASSERT_EQ(json["tables"].size(), 1U);
  EXPECT_EQ(json["tables"][0]["name"].asString(), "candles");
  ASSERT_EQ(json["tables"][0]["columns"].size(), 2U);
  EXPECT_EQ(json["tables"][0]["columns"][0]["name"].asString(), "symbol");
  EXPECT_FALSE(json["tables"][0]["columns"][0]["is_nullable"].asBool());
}

} // namespace
} // namespace howling::reader_mcp
