#include "services/db/reader/postgres_reader_db.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "libpq/libpq-fe.h"
#include "services/db/pg/connection.h"
#include "services/db/pg/query.h"
#include "services/db/reader/sql_builder.h"

namespace howling {
namespace {

constexpr int64_t MICROSECONDS_PER_SECOND = 1'000'000;

template <typename Page>
void finalize_page(Page& page, int32_t limit) {
  if (page.rows.size() > static_cast<size_t>(limit)) {
    page.has_more = true;
    page.rows.pop_back();
  }
}

} // namespace

struct postgres_reader_db::implementation {
  pg::unique_conn conn;
};

postgres_reader_db::postgres_reader_db(postgres_options options)
    : _implementation{std::make_unique<implementation>()} {
  _implementation->conn = pg::connect(options);
  pg::execute(
      *_implementation->conn,
      R"sql(SET default_transaction_read_only = on)sql");
  pg::execute(*_implementation->conn, R"sql(SET statement_timeout = '10s')sql");
}

postgres_reader_db::~postgres_reader_db() = default;

PGconn& postgres_reader_db::_connection() {
  return *_implementation->conn;
}

bool postgres_reader_db::is_healthy() const {
  if (!_implementation || !_implementation->conn) return false;
  return PQstatus(_implementation->conn.get()) == CONNECTION_OK;
}

void postgres_reader_db::reset() {
  if (!_implementation || !_implementation->conn) return;
  pg::rollback_if_in_transaction(*_implementation->conn);
}

candle_page postgres_reader_db::read_candles(const candle_query& query) {
  reader_internal::built_query built =
      reader_internal::build_candles_sql(query);
  pg::query pg_query{
      *_implementation->conn, built.sql, std::move(built.params)};

  candle_page page;
  while (pg_query.step()) {
    candle_row row;
    if (query.bucket_seconds) {
      pg_query.read_all(
          row.symbol_id,
          row.opened_at,
          row.open,
          row.close,
          row.high,
          row.low,
          row.volume);
      row.duration_us = *query.bucket_seconds * MICROSECONDS_PER_SECOND;
    } else {
      pg_query.read_all(
          row.symbol_id,
          row.opened_at,
          row.open,
          row.close,
          row.high,
          row.low,
          row.volume,
          row.duration_us);
    }
    page.rows.push_back(std::move(row));
  }

  finalize_page(page, query.limit);
  return page;
}

market_page postgres_reader_db::read_market_data(const market_query& query) {
  reader_internal::built_query built =
      reader_internal::build_market_data_sql(query);
  pg::query pg_query{
      *_implementation->conn, built.sql, std::move(built.params)};

  market_page page;
  while (pg_query.step()) {
    market_row row;
    pg_query.read_all(
        row.symbol_id,
        row.emitted_at,
        row.bid,
        row.bid_lots,
        row.ask,
        row.ask_lots,
        row.last,
        row.last_lots);
    page.rows.push_back(std::move(row));
  }

  finalize_page(page, query.limit);
  return page;
}

trade_page postgres_reader_db::read_trades(const trade_query& query) {
  reader_internal::built_query built = reader_internal::build_trades_sql(query);
  pg::query pg_query{
      *_implementation->conn, built.sql, std::move(built.params)};

  trade_page page;
  while (pg_query.step()) {
    trade_row row;
    pg_query.read_all(
        row.executed_at,
        row.symbol_id,
        row.symbol_name,
        row.action_id,
        row.action_name,
        row.price,
        row.quantity,
        row.confidence,
        row.dry_run);
    page.rows.push_back(std::move(row));
  }

  finalize_page(page, query.limit);
  return page;
}

std::vector<trading_summary_row>
postgres_reader_db::read_trading_summary(const summary_query& query) {
  reader_internal::built_query built =
      reader_internal::build_trading_summary_sql(query);
  pg::query pg_query{
      *_implementation->conn, built.sql, std::move(built.params)};

  std::vector<trading_summary_row> rows;
  while (pg_query.step()) {
    trading_summary_row row;
    pg_query.read_all(
        row.symbol_id,
        row.symbol_name,
        row.total_trades,
        row.buy_count,
        row.sell_count,
        row.hold_count,
        row.buy_quantity,
        row.sell_quantity,
        row.buy_notional,
        row.sell_notional,
        row.dry_run_count,
        row.live_count,
        row.avg_confidence,
        row.first_trade_at,
        row.last_trade_at,
        row.candle_count,
        row.open,
        row.close,
        row.high,
        row.low,
        row.total_volume);
    rows.push_back(std::move(row));
  }
  return rows;
}

symbols_metadata postgres_reader_db::read_symbols_metadata() {
  symbols_metadata metadata;

  pg::query coverage_query{
      *_implementation->conn, reader_internal::symbols_coverage_sql()};
  while (coverage_query.step()) {
    symbol_metadata row;
    coverage_query.read_all(
        row.symbol_id,
        row.symbol_name,
        row.min_candle_at,
        row.max_candle_at,
        row.min_market_at,
        row.max_market_at,
        row.min_trade_at,
        row.max_trade_at,
        row.trade_count);
    metadata.symbols.push_back(std::move(row));
  }

  pg::query estimates_query{
      *_implementation->conn, reader_internal::table_estimates_sql()};
  while (estimates_query.step()) {
    table_row_estimate row;
    estimates_query.read_all(row.table_name, row.approx_rows);
    metadata.table_estimates.push_back(std::move(row));
  }

  return metadata;
}

std::vector<action_info> postgres_reader_db::read_actions_metadata() {
  pg::query pg_query{
      *_implementation->conn, reader_internal::actions_metadata_sql()};
  std::vector<action_info> actions;
  while (pg_query.step()) {
    action_info info;
    pg_query.read_all(info.id, info.name);
    actions.push_back(std::move(info));
  }
  return actions;
}

schema_metadata postgres_reader_db::read_schema_metadata() {
  schema_metadata metadata;
  pg::execute_read(
      *_implementation->conn,
      reader_internal::schema_version_sql(),
      metadata.schema_version);

  pg::query columns_query{
      *_implementation->conn, reader_internal::schema_columns_sql()};
  while (columns_query.step()) {
    std::string table_name;
    column_metadata col;
    columns_query.read_all(
        table_name, col.column_name, col.data_type, col.is_nullable);
    if (metadata.tables.empty() ||
        metadata.tables.back().table_name != table_name) {
      metadata.tables.push_back(
          table_metadata{.table_name = std::move(table_name), .columns = {}});
    }
    metadata.tables.back().columns.push_back(std::move(col));
  }

  return metadata;
}

} // namespace howling
