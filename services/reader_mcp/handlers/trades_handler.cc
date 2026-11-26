#include "services/reader_mcp/handlers/trades_handler.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "services/db/reader/reader_db.h"
#include "services/mcp/tool_handler.h"
#include "services/reader_mcp/handlers/validation.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::reader_mcp {

trades_handler::trades_handler(reader_db_pool& db_pool) : _db_pool(db_pool) {}

std::string_view trades_handler::name() const {
  return "get_trades";
}

std::string_view trades_handler::description() const {
  return "Query executed trade records with optional symbol, action, dry_run, "
         "and time range filters.";
}

Json::Value trades_handler::input_schema() const {
  Json::Value properties(Json::objectValue);
  add_symbol_schema(
      properties, "Optional uppercase stock symbol enum filter (e.g. NVDA).");

  Json::Value action(Json::objectValue);
  action["type"] = "string";
  action["enum"] = Json::Value(Json::arrayValue);
  action["enum"].append("BUY");
  action["enum"].append("SELL");
  action["enum"].append("HOLD");
  action["description"] = "Optional uppercase trade action filter.";
  properties["action"] = std::move(action);

  add_dry_run_schema(properties);
  add_time_range_schema(properties);
  add_pagination_schema(properties);

  return make_tool_input_schema(std::move(properties));
}

mcp::tool_annotations trades_handler::annotations() const {
  return mcp::tool_annotations{.read_only_hint = true};
}

mcp::tool_result trades_handler::call(const Json::Value& arguments) {
  validate_allowed_arguments(
      arguments,
      {"symbol",
       "action",
       "dry_run",
       "start_time",
       "end_time",
       "limit",
       "offset",
       "order"});

  const time_range range = parse_time_range(arguments);
  const pagination_params pagination = parse_pagination(arguments);

  trade_query query;
  if (const std::optional<std::string> symbol_name =
          parse_optional_string(arguments, "symbol");
      symbol_name.has_value()) {
    query.symbol_id = parse_symbol(*symbol_name);
  }
  if (const std::optional<std::string> action_name =
          parse_optional_string(arguments, "action");
      action_name.has_value()) {
    query.action_id = parse_action(*action_name);
  }
  query.dry_run = parse_optional_bool(arguments, "dry_run");
  query.start_time = range.start_time;
  query.end_time = range.end_time;
  query.order = parse_sort_order(arguments);
  query.limit = pagination.limit;
  query.offset = pagination.offset;

  const reader_db_pool::scoped_resource db = _db_pool.acquire();
  const trade_page page = db->read_trades(query);

  Json::Value trades(Json::arrayValue);
  for (const trade_row& row : page.rows) {
    Json::Value item(Json::objectValue);
    item["executed_at"] = format_utc_timestamp(row.executed_at);
    item["symbol"] = row.symbol_name.empty()
        ? symbol_name_from_id(row.symbol_id)
        : row.symbol_name;
    item["action"] = row.action_name.empty()
        ? action_name_from_id(row.action_id)
        : row.action_name;
    item["price"] = row.price;
    item["quantity"] = Json::Int64{row.quantity};
    item["confidence"] = row.confidence;
    item["dry_run"] = row.dry_run;
    trades.append(std::move(item));
  }

  Json::Value payload(Json::objectValue);
  payload["trades"] = std::move(trades);
  payload["count"] = Json::Int64{static_cast<int64_t>(page.rows.size())};
  payload["limit"] = query.limit;
  payload["offset"] = query.offset;
  payload["has_more"] = page.has_more;

  return mcp::tool_result::ok(to_string(payload));
}

} // namespace howling::reader_mcp
