#include "services/reader_mcp/handlers/trading_summary_handler.h"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/db/reader/reader_db.h"
#include "services/mcp/tool_handler.h"
#include "services/reader_mcp/handlers/validation.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::reader_mcp {

trading_summary_handler::trading_summary_handler(
    reader_db_pool& db_pool, clock_callback clock)
    : _db_pool(db_pool), _clock(std::move(clock)) {
  if (!_clock) {
    throw std::invalid_argument("Clock callback must not be empty.");
  }
}

std::string_view trading_summary_handler::name() const {
  return "get_trading_summary";
}

std::string_view trading_summary_handler::description() const {
  return "Return per-symbol trade and candle aggregates (defaults to the last "
         "24 hours UTC when both start_time and end_time are omitted).";
}

Json::Value trading_summary_handler::input_schema() const {
  Json::Value properties(Json::objectValue);
  add_symbol_schema(
      properties, "Optional uppercase stock symbol enum filter (e.g. NVDA).");
  add_dry_run_schema(properties);
  add_time_range_schema(
      properties,
      "Defaults to 24 hours before now when both start_time and end_time are "
      "omitted.",
      "Defaults to now UTC when both start_time and end_time are omitted.");

  return make_tool_input_schema(std::move(properties));
}

mcp::tool_annotations trading_summary_handler::annotations() const {
  return mcp::tool_annotations{.read_only_hint = true};
}

mcp::tool_result trading_summary_handler::call(const Json::Value& arguments) {
  validate_allowed_arguments(
      arguments, {"symbol", "dry_run", "start_time", "end_time"});

  time_range range = parse_time_range(arguments);
  if (!range.start_time.has_value() && !range.end_time.has_value()) {
    const std::chrono::system_clock::time_point now = _clock();
    range.end_time = now;
    range.start_time = now - DEFAULT_SUMMARY_WINDOW;
  }

  summary_query query;
  if (const std::optional<std::string> symbol_name =
          parse_optional_string(arguments, "symbol");
      symbol_name.has_value()) {
    query.symbol_id = parse_symbol(*symbol_name);
  }
  query.dry_run = parse_optional_bool(arguments, "dry_run");
  query.start_time = range.start_time;
  query.end_time = range.end_time;

  const reader_db_pool::scoped_resource db = _db_pool.acquire();
  const std::vector<trading_summary_row> rows = db->read_trading_summary(query);

  Json::Value summaries(Json::arrayValue);
  for (const trading_summary_row& row : rows) {
    Json::Value item(Json::objectValue);
    item["symbol"] = row.symbol_name.empty()
        ? symbol_name_from_id(row.symbol_id)
        : row.symbol_name;
    item["total_trades"] = Json::Int64{row.total_trades};
    item["buy_count"] = Json::Int64{row.buy_count};
    item["sell_count"] = Json::Int64{row.sell_count};
    item["hold_count"] = Json::Int64{row.hold_count};
    item["buy_quantity"] = Json::Int64{row.buy_quantity};
    item["sell_quantity"] = Json::Int64{row.sell_quantity};
    item["buy_notional"] = row.buy_notional;
    item["sell_notional"] = row.sell_notional;
    item["dry_run_count"] = Json::Int64{row.dry_run_count};
    item["live_count"] = Json::Int64{row.live_count};
    item["avg_confidence"] = to_json_double_or_null(row.avg_confidence);
    item["first_trade_at"] = to_json_timestamp_or_null(row.first_trade_at);
    item["last_trade_at"] = to_json_timestamp_or_null(row.last_trade_at);
    item["candle_count"] = Json::Int64{row.candle_count};
    item["open"] = to_json_double_or_null(row.open);
    item["close"] = to_json_double_or_null(row.close);
    item["high"] = to_json_double_or_null(row.high);
    item["low"] = to_json_double_or_null(row.low);
    item["total_volume"] = Json::Int64{row.total_volume};
    summaries.append(std::move(item));
  }

  Json::Value payload(Json::objectValue);
  payload["start_time"] = to_json_timestamp_or_null(query.start_time);
  payload["end_time"] = to_json_timestamp_or_null(query.end_time);
  payload["summaries"] = std::move(summaries);

  return mcp::tool_result::ok(to_string(payload));
}

} // namespace howling::reader_mcp
