#include "services/reader_mcp/handlers/candles_handler.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "services/db/reader/reader_db.h"
#include "services/mcp/tool_handler.h"
#include "services/reader_mcp/handlers/validation.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::reader_mcp {

static_assert(
    MAX_BUCKET_SECONDS <= std::numeric_limits<int64_t>::max() / 1'000'000,
    "MAX_BUCKET_SECONDS must not overflow microsecond conversion.");

candles_handler::candles_handler(reader_db_pool& db_pool) : _db_pool(db_pool) {}

std::string_view candles_handler::name() const {
  return "get_candles";
}

std::string_view candles_handler::description() const {
  return "Query raw or time-bucketed OHLCV candles for a stock symbol.";
}

Json::Value candles_handler::input_schema() const {
  Json::Value properties(Json::objectValue);
  add_symbol_schema(
      properties,
      "Uppercase stock symbol enum name (e.g. NVDA, AAPL, AMD, MU, RIVN).");
  add_time_range_schema(properties);
  add_pagination_schema(properties);

  Json::Value bucket_seconds(Json::objectValue);
  bucket_seconds["type"] = "integer";
  bucket_seconds["minimum"] = 1;
  bucket_seconds["maximum"] = Json::Int64{MAX_BUCKET_SECONDS};
  bucket_seconds["description"] =
      "Optional bucket width in seconds for server-side OHLCV aggregation "
      "(1-" +
      std::to_string(MAX_BUCKET_SECONDS) + ").";
  properties["bucket_seconds"] = std::move(bucket_seconds);

  return make_tool_input_schema(std::move(properties), {"symbol"});
}

mcp::tool_annotations candles_handler::annotations() const {
  return mcp::tool_annotations{.read_only_hint = true};
}

mcp::tool_result candles_handler::call(const Json::Value& arguments) {
  validate_allowed_arguments(
      arguments,
      {"symbol",
       "start_time",
       "end_time",
       "limit",
       "offset",
       "order",
       "bucket_seconds"});

  const std::string symbol_name = parse_required_string(arguments, "symbol");
  const time_range range = parse_time_range(arguments);
  const pagination_params pagination = parse_pagination(arguments);

  candle_query query;
  query.symbol_id = parse_symbol(symbol_name);
  query.start_time = range.start_time;
  query.end_time = range.end_time;
  query.order = parse_sort_order(arguments);
  query.limit = pagination.limit;
  query.offset = pagination.offset;
  query.bucket_seconds =
      parse_optional_positive_int64(arguments, "bucket_seconds");
  if (query.bucket_seconds.has_value() &&
      *query.bucket_seconds > MAX_BUCKET_SECONDS) {
    throw mcp::tool_error(
        "Argument 'bucket_seconds' must be at most " +
        std::to_string(MAX_BUCKET_SECONDS) + ".");
  }

  const reader_db_pool::scoped_resource db = _db_pool.acquire();
  const candle_page page = db->read_candles(query);

  Json::Value candles(Json::arrayValue);
  for (const candle_row& row : page.rows) {
    Json::Value item(Json::objectValue);
    item["symbol"] = symbol_name_from_id(row.symbol_id);
    item["opened_at"] = format_utc_timestamp(row.opened_at);
    item["open"] = row.open;
    item["close"] = row.close;
    item["high"] = row.high;
    item["low"] = row.low;
    item["volume"] = Json::Int64{row.volume};
    item["duration_us"] = Json::Int64{row.duration_us};
    candles.append(std::move(item));
  }

  Json::Value payload(Json::objectValue);
  payload["symbol"] = symbol_name;
  if (query.bucket_seconds.has_value()) {
    payload["bucket_seconds"] = Json::Int64{*query.bucket_seconds};
  }
  payload["candles"] = std::move(candles);
  payload["count"] = Json::Int64{static_cast<int64_t>(page.rows.size())};
  payload["limit"] = query.limit;
  payload["offset"] = query.offset;
  payload["has_more"] = page.has_more;

  return mcp::tool_result::ok(to_string(payload));
}

} // namespace howling::reader_mcp
