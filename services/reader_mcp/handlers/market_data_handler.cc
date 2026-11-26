#include "services/reader_mcp/handlers/market_data_handler.h"

#include <string>
#include <string_view>
#include <utility>

#include "services/db/reader/reader_db.h"
#include "services/mcp/tool_handler.h"
#include "services/reader_mcp/handlers/validation.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::reader_mcp {

market_data_handler::market_data_handler(reader_db_pool& db_pool)
    : _db_pool(db_pool) {}

std::string_view market_data_handler::name() const {
  return "get_market_data";
}

std::string_view market_data_handler::description() const {
  return "Query Level-1 market quotes (bid, ask, last) for a stock symbol.";
}

Json::Value market_data_handler::input_schema() const {
  Json::Value properties(Json::objectValue);
  add_symbol_schema(
      properties,
      "Uppercase stock symbol enum name (e.g. NVDA, AAPL, AMD, MU, RIVN).");
  add_time_range_schema(properties);
  add_pagination_schema(properties);

  return make_tool_input_schema(std::move(properties), {"symbol"});
}

mcp::tool_annotations market_data_handler::annotations() const {
  return mcp::tool_annotations{.read_only_hint = true};
}

mcp::tool_result market_data_handler::call(const Json::Value& arguments) {
  validate_allowed_arguments(
      arguments,
      {"symbol", "start_time", "end_time", "limit", "offset", "order"});

  const std::string symbol_name = parse_required_string(arguments, "symbol");
  const time_range range = parse_time_range(arguments);
  const pagination_params pagination = parse_pagination(arguments);

  market_query query;
  query.symbol_id = parse_symbol(symbol_name);
  query.start_time = range.start_time;
  query.end_time = range.end_time;
  query.order = parse_sort_order(arguments);
  query.limit = pagination.limit;
  query.offset = pagination.offset;

  const reader_db_pool::scoped_resource db = _db_pool.acquire();
  const market_page page = db->read_market_data(query);

  Json::Value quotes(Json::arrayValue);
  for (const market_row& row : page.rows) {
    Json::Value item(Json::objectValue);
    item["symbol"] = symbol_name_from_id(row.symbol_id);
    item["emitted_at"] = format_utc_timestamp(row.emitted_at);
    item["bid"] = row.bid;
    item["bid_lots"] = Json::Int64{row.bid_lots};
    item["ask"] = row.ask;
    item["ask_lots"] = Json::Int64{row.ask_lots};
    item["last"] = row.last;
    item["last_lots"] = Json::Int64{row.last_lots};
    quotes.append(std::move(item));
  }

  Json::Value payload(Json::objectValue);
  payload["symbol"] = symbol_name;
  payload["quotes"] = std::move(quotes);
  payload["count"] = Json::Int64{static_cast<int64_t>(page.rows.size())};
  payload["limit"] = query.limit;
  payload["offset"] = query.offset;
  payload["has_more"] = page.has_more;

  return mcp::tool_result::ok(to_string(payload));
}

} // namespace howling::reader_mcp
