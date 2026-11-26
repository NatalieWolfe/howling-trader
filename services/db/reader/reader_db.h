#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "containers/resource_pool.h"
#include "services/service_base.h"

namespace howling {

/**
 * @brief Sort direction for time-ordered reader queries.
 */
enum class sort_order {
  ASC,
  DESC,
};

/**
 * @brief Filter and pagination parameters for reading candles.
 *
 * Time bounds are inclusive: opened_at >= start_time and opened_at <= end_time.
 */
struct candle_query {
  int32_t symbol_id = 0;
  std::optional<std::chrono::system_clock::time_point> start_time;
  std::optional<std::chrono::system_clock::time_point> end_time;
  sort_order order = sort_order::DESC;
  int32_t limit = 100;
  int32_t offset = 0;
  std::optional<int64_t> bucket_seconds;
};

/**
 * @brief Single raw or bucketed OHLCV candle row.
 */
struct candle_row {
  int32_t symbol_id = 0;
  std::chrono::system_clock::time_point opened_at;
  double open = 0.0;
  double close = 0.0;
  double high = 0.0;
  double low = 0.0;
  int64_t volume = 0;
  int64_t duration_us = 0;
};

/**
 * @brief Paginated result of a candle query.
 */
struct candle_page {
  std::vector<candle_row> rows;
  bool has_more = false;
};

/**
 * @brief Filter and pagination parameters for reading market ticks.
 *
 * Time bounds are inclusive: emitted_at >= start_time and
 * emitted_at <= end_time.
 */
struct market_query {
  int32_t symbol_id = 0;
  std::optional<std::chrono::system_clock::time_point> start_time;
  std::optional<std::chrono::system_clock::time_point> end_time;
  sort_order order = sort_order::DESC;
  int32_t limit = 100;
  int32_t offset = 0;
};

/**
 * @brief Single market quote row.
 */
struct market_row {
  int32_t symbol_id = 0;
  std::chrono::system_clock::time_point emitted_at;
  double bid = 0.0;
  int64_t bid_lots = 0;
  double ask = 0.0;
  int64_t ask_lots = 0;
  double last = 0.0;
  int64_t last_lots = 0;
};

/**
 * @brief Paginated result of a market data query.
 */
struct market_page {
  std::vector<market_row> rows;
  bool has_more = false;
};

/**
 * @brief Filter and pagination parameters for reading trades.
 *
 * Time bounds are inclusive: executed_at >= start_time and
 * executed_at <= end_time.
 */
struct trade_query {
  std::optional<int32_t> symbol_id;
  std::optional<int32_t> action_id;
  std::optional<bool> dry_run;
  std::optional<std::chrono::system_clock::time_point> start_time;
  std::optional<std::chrono::system_clock::time_point> end_time;
  sort_order order = sort_order::DESC;
  int32_t limit = 100;
  int32_t offset = 0;
};

/**
 * @brief Single trade row with resolved symbol and action names.
 */
struct trade_row {
  std::chrono::system_clock::time_point executed_at;
  int32_t symbol_id = 0;
  std::string symbol_name;
  int32_t action_id = 0;
  std::string action_name;
  double price = 0.0;
  int64_t quantity = 0;
  double confidence = 0.0;
  bool dry_run = false;
};

/**
 * @brief Paginated result of a trades query.
 */
struct trade_page {
  std::vector<trade_row> rows;
  bool has_more = false;
};

/**
 * @brief Filter parameters for aggregated trading and candle summaries.
 *
 * Time bounds are inclusive across both trades and candles.
 */
struct summary_query {
  std::optional<int32_t> symbol_id;
  std::optional<bool> dry_run;
  std::optional<std::chrono::system_clock::time_point> start_time;
  std::optional<std::chrono::system_clock::time_point> end_time;
};

/**
 * @brief Aggregated trade and candle summary for a single symbol.
 */
struct trading_summary_row {
  int32_t symbol_id = 0;
  std::string symbol_name;

  int64_t total_trades = 0;
  int64_t buy_count = 0;
  int64_t sell_count = 0;
  int64_t hold_count = 0;
  int64_t buy_quantity = 0;
  int64_t sell_quantity = 0;
  double buy_notional = 0.0;
  double sell_notional = 0.0;
  int64_t dry_run_count = 0;
  int64_t live_count = 0;
  std::optional<double> avg_confidence;
  std::optional<std::chrono::system_clock::time_point> first_trade_at;
  std::optional<std::chrono::system_clock::time_point> last_trade_at;

  int64_t candle_count = 0;
  std::optional<double> open;
  std::optional<double> close;
  std::optional<double> high;
  std::optional<double> low;
  int64_t total_volume = 0;
};

/**
 * @brief Per-symbol timestamp coverage and trade count metadata.
 */
struct symbol_metadata {
  int32_t symbol_id = 0;
  std::string symbol_name;
  std::optional<std::chrono::system_clock::time_point> min_candle_at;
  std::optional<std::chrono::system_clock::time_point> max_candle_at;
  std::optional<std::chrono::system_clock::time_point> min_market_at;
  std::optional<std::chrono::system_clock::time_point> max_market_at;
  std::optional<std::chrono::system_clock::time_point> min_trade_at;
  std::optional<std::chrono::system_clock::time_point> max_trade_at;
  int64_t trade_count = 0;
};

/**
 * @brief Approximate row count for a table from pg_class.reltuples.
 */
struct table_row_estimate {
  std::string table_name;
  int64_t approx_rows = 0;
};

/**
 * @brief Symbol coverage metadata paired with approximate table row counts.
 */
struct symbols_metadata {
  std::vector<symbol_metadata> symbols;
  std::vector<table_row_estimate> table_estimates;
};

/**
 * @brief Action enum entry from enum_actions.
 */
struct action_info {
  int32_t id = 0;
  std::string name;
};

/**
 * @brief Column description from information_schema.columns.
 */
struct column_metadata {
  std::string column_name;
  std::string data_type;
  bool is_nullable = false;
};

/**
 * @brief Table description with its ordered columns.
 */
struct table_metadata {
  std::string table_name;
  std::vector<column_metadata> columns;
};

/**
 * @brief Database schema version and allowlisted table/column overview.
 */
struct schema_metadata {
  int32_t schema_version = 0;
  std::vector<table_metadata> tables;
};

/**
 * @brief Read-only database interface for analytics and MCP reader services.
 *
 * Implementations are thread-compatible (one query at a time per connection).
 * Use reader_db_pool for concurrent multi-threaded access.
 */
class reader_db : public service, public poolable_resource {
public:
  virtual ~reader_db() = default;

  /**
   * @brief Reads raw or time-bucketed OHLCV candles for a symbol.
   */
  [[nodiscard]] virtual candle_page read_candles(const candle_query& query) = 0;

  /**
   * @brief Reads market quote ticks for a symbol.
   */
  [[nodiscard]] virtual market_page
  read_market_data(const market_query& query) = 0;

  /**
   * @brief Reads trade records matching optional filters.
   */
  [[nodiscard]] virtual trade_page read_trades(const trade_query& query) = 0;

  /**
   * @brief Reads per-symbol trade and candle aggregates, including symbols
   * with candles and zero trades.
   */
  [[nodiscard]] virtual std::vector<trading_summary_row>
  read_trading_summary(const summary_query& query) = 0;

  /**
   * @brief Reads per-symbol timestamp bounds, trade counts, and approximate
   * table row counts.
   */
  [[nodiscard]] virtual symbols_metadata read_symbols_metadata() = 0;

  /**
   * @brief Reads all action enum definitions from enum_actions.
   */
  [[nodiscard]] virtual std::vector<action_info> read_actions_metadata() = 0;

  /**
   * @brief Reads the schema version and allowlisted table/column metadata.
   */
  [[nodiscard]] virtual schema_metadata read_schema_metadata() = 0;
};

/**
 * @brief Thread-safe connection pool of read-only reader_db instances.
 */
class reader_db_pool : public service {
public:
  using options = resource_pool<reader_db>::options;
  using scoped_resource = resource_pool<reader_db>::scoped_resource;

  explicit reader_db_pool(
      resource_pool<reader_db>::resource_factory factory,
      options pool_options = {})
      : _pool{std::move(factory), pool_options} {}

  [[nodiscard]] scoped_resource acquire() { return _pool.acquire(); }

  [[nodiscard]] resource_pool<reader_db>& pool() noexcept { return _pool; }
  [[nodiscard]] const resource_pool<reader_db>& pool() const noexcept {
    return _pool;
  }

private:
  resource_pool<reader_db> _pool;
};

} // namespace howling
