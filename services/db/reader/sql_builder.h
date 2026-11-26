#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "services/db/pg/query.h"
#include "services/db/reader/reader_db.h"

namespace howling::reader_internal {

/**
 * @brief Parameterized SQL paired with its exact bound binary parameters.
 */
struct built_query {
  std::string sql;
  pg::param_binder params;
};

/**
 * @brief Validates pagination parameters (limit > 0, offset >= 0).
 *
 * @throws std::invalid_argument if limit <= 0 or offset < 0.
 */
void validate_pagination(int32_t limit, int32_t offset);

/**
 * @brief Builds parameterized SQL and bound parameters for reading raw or
 * bucketed candles.
 *
 * @throws std::invalid_argument if pagination or bucket_seconds is invalid or
 *   would overflow microsecond duration calculations.
 */
[[nodiscard]] built_query build_candles_sql(const candle_query& query);

/**
 * @brief Builds parameterized SQL and bound parameters for reading market
 * ticks.
 *
 * @throws std::invalid_argument if pagination is invalid.
 */
[[nodiscard]] built_query build_market_data_sql(const market_query& query);

/**
 * @brief Builds parameterized SQL and bound parameters for reading trades
 * using index-friendly integer predicates on the trades table.
 *
 * @throws std::invalid_argument if pagination is invalid.
 */
[[nodiscard]] built_query build_trades_sql(const trade_query& query);

/**
 * @brief Builds parameterized SQL and bound parameters for per-symbol trading
 * and candle summaries.
 */
[[nodiscard]] built_query build_trading_summary_sql(const summary_query& query);

/**
 * @brief Returns SQL for per-symbol timestamp bounds and exact trade counts
 * using LATERAL index lookups.
 */
[[nodiscard]] std::string_view symbols_coverage_sql();

/**
 * @brief Returns SQL for approximate table row counts from pg_class.reltuples.
 */
[[nodiscard]] std::string_view table_estimates_sql();

/**
 * @brief Returns SQL for reading enum_actions metadata.
 */
[[nodiscard]] std::string_view actions_metadata_sql();

/**
 * @brief Returns SQL for reading the current schema version.
 */
[[nodiscard]] std::string_view schema_version_sql();

/**
 * @brief Returns SQL for reading allowlisted table/column metadata from
 * information_schema.columns.
 */
[[nodiscard]] std::string_view schema_columns_sql();

} // namespace howling::reader_internal
