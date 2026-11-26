#include "services/reader_mcp/handlers/symbols_resource_handler.h"

#include <string>
#include <string_view>
#include <utility>

#include "services/db/reader/reader_db.h"
#include "services/reader_mcp/handlers/validation.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::reader_mcp {

symbols_resource_handler::symbols_resource_handler(reader_db_pool& db_pool)
    : _db_pool(db_pool) {}

std::string_view symbols_resource_handler::uri() const {
  return "howling://symbols";
}

std::string_view symbols_resource_handler::name() const {
  return "symbols";
}

std::string_view symbols_resource_handler::description() const {
  return "Available stock symbols, per-symbol timestamp coverage, exact trade "
         "counts, and approximate table row counts.";
}

std::string_view symbols_resource_handler::mime_type() const {
  return "application/json";
}

std::string symbols_resource_handler::read() {
  const reader_db_pool::scoped_resource db = _db_pool.acquire();
  const symbols_metadata metadata = db->read_symbols_metadata();

  Json::Value symbols(Json::arrayValue);
  for (const symbol_metadata& entry : metadata.symbols) {
    Json::Value item(Json::objectValue);
    item["id"] = entry.symbol_id;
    item["symbol"] = entry.symbol_name;
    item["min_candle_at"] = to_json_timestamp_or_null(entry.min_candle_at);
    item["max_candle_at"] = to_json_timestamp_or_null(entry.max_candle_at);
    item["min_market_at"] = to_json_timestamp_or_null(entry.min_market_at);
    item["max_market_at"] = to_json_timestamp_or_null(entry.max_market_at);
    item["min_trade_at"] = to_json_timestamp_or_null(entry.min_trade_at);
    item["max_trade_at"] = to_json_timestamp_or_null(entry.max_trade_at);
    item["trade_count"] = Json::Int64{entry.trade_count};
    symbols.append(std::move(item));
  }

  Json::Value table_estimates(Json::arrayValue);
  for (const table_row_estimate& estimate : metadata.table_estimates) {
    Json::Value item(Json::objectValue);
    item["table"] = estimate.table_name;
    item["approx_rows"] = Json::Int64{estimate.approx_rows};
    table_estimates.append(std::move(item));
  }

  Json::Value payload(Json::objectValue);
  payload["symbols"] = std::move(symbols);
  payload["table_estimates"] = std::move(table_estimates);
  return to_string(payload);
}

} // namespace howling::reader_mcp
