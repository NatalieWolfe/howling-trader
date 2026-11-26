#include "services/db/reader/sql_builder.h"

#include <chrono>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "data/trade.pb.h"

namespace howling::reader_internal {
namespace {

constexpr int64_t MICROSECONDS_PER_SECOND = 1'000'000;
constexpr int64_t MAX_BUCKET_SECONDS =
    std::numeric_limits<int64_t>::max() / MICROSECONDS_PER_SECOND;

constexpr int32_t HOLD_ACTION_ID =
    static_cast<int32_t>(howling::trading::Action::HOLD);
constexpr int32_t BUY_ACTION_ID =
    static_cast<int32_t>(howling::trading::Action::BUY);
constexpr int32_t SELL_ACTION_ID =
    static_cast<int32_t>(howling::trading::Action::SELL);

std::string_view order_direction(sort_order order) {
  return order == sort_order::ASC ? "ASC" : "DESC";
}

class query_builder {
public:
  template <typename T>
  [[nodiscard]] std::string bind(T&& value) {
    _params.bind(std::forward<T>(value));
    return std::format("${}", _params.count());
  }

  [[nodiscard]] built_query finish(std::string sql) {
    return built_query{
        .sql = std::move(sql),
        .params = std::move(_params),
    };
  }

private:
  pg::param_binder _params;
};

void append_time_range_and_pagination(
    std::string& sql,
    query_builder& builder,
    std::string_view time_column,
    const std::optional<std::chrono::system_clock::time_point>& start_time,
    const std::optional<std::chrono::system_clock::time_point>& end_time,
    std::string_view group_by_clause,
    std::string_view order_column,
    sort_order order,
    int32_t limit,
    int32_t offset) {
  if (start_time) {
    sql += std::format(" AND {} >= {}", time_column, builder.bind(*start_time));
  }
  if (end_time) {
    sql += std::format(" AND {} <= {}", time_column, builder.bind(*end_time));
  }
  if (!group_by_clause.empty()) {
    sql += " ";
    sql += group_by_clause;
  }
  std::string limit_param = builder.bind(static_cast<int64_t>(limit) + 1);
  std::string offset_param = builder.bind(static_cast<int64_t>(offset));
  sql += std::format(
      " ORDER BY {} {} LIMIT {} OFFSET {}",
      order_column,
      order_direction(order),
      limit_param,
      offset_param);
}

} // namespace

void validate_pagination(int32_t limit, int32_t offset) {
  if (limit <= 0) {
    throw std::invalid_argument("Query limit must be greater than zero.");
  }
  if (offset < 0) {
    throw std::invalid_argument("Query offset must be non-negative.");
  }
}

built_query build_candles_sql(const candle_query& query) {
  validate_pagination(query.limit, query.offset);
  if (query.bucket_seconds) {
    if (*query.bucket_seconds <= 0) {
      throw std::invalid_argument("bucket_seconds must be greater than zero.");
    }
    if (*query.bucket_seconds > MAX_BUCKET_SECONDS) {
      throw std::invalid_argument(
          "bucket_seconds exceeds maximum representable duration.");
    }
  }

  query_builder builder;
  std::string sql;
  if (query.bucket_seconds) {
    std::string bucket_param = builder.bind(*query.bucket_seconds);
    std::string symbol_param = builder.bind(query.symbol_id);
    sql = std::format(
        R"sql(
          SELECT
            symbol,
            date_bin(
              make_interval(secs => {0}::DOUBLE PRECISION),
              opened_at,
              TIMESTAMP '1970-01-01 00:00:00'
            ) AS bucket_at,
            (ARRAY_AGG(open ORDER BY opened_at ASC))[1]::DOUBLE PRECISION
              AS open,
            (ARRAY_AGG(close ORDER BY opened_at DESC))[1]::DOUBLE PRECISION
              AS close,
            MAX(high)::DOUBLE PRECISION AS high,
            MIN(low)::DOUBLE PRECISION AS low,
            COALESCE(SUM(volume), 0)::BIGINT AS volume
          FROM candles
          WHERE symbol = {1})sql",
        bucket_param,
        symbol_param);
    append_time_range_and_pagination(
        sql,
        builder,
        "opened_at",
        query.start_time,
        query.end_time,
        "GROUP BY symbol, bucket_at",
        "bucket_at",
        query.order,
        query.limit,
        query.offset);
  } else {
    std::string symbol_param = builder.bind(query.symbol_id);
    sql = std::format(
        R"sql(
          SELECT
            symbol, opened_at, open, close, high, low, volume, duration_us
          FROM candles
          WHERE symbol = {})sql",
        symbol_param);
    append_time_range_and_pagination(
        sql,
        builder,
        "opened_at",
        query.start_time,
        query.end_time,
        "",
        "opened_at",
        query.order,
        query.limit,
        query.offset);
  }
  return builder.finish(std::move(sql));
}

built_query build_market_data_sql(const market_query& query) {
  validate_pagination(query.limit, query.offset);

  query_builder builder;
  std::string symbol_param = builder.bind(query.symbol_id);
  std::string sql = std::format(
      R"sql(
        SELECT
          symbol, emitted_at, bid, bid_lots, ask, ask_lots, last, last_lots
        FROM market
        WHERE symbol = {})sql",
      symbol_param);
  append_time_range_and_pagination(
      sql,
      builder,
      "emitted_at",
      query.start_time,
      query.end_time,
      "",
      "emitted_at",
      query.order,
      query.limit,
      query.offset);
  return builder.finish(std::move(sql));
}

built_query build_trades_sql(const trade_query& query) {
  validate_pagination(query.limit, query.offset);

  query_builder builder;
  std::string sql = R"sql(
    SELECT
      t.executed_at,
      t.symbol,
      COALESCE(s.name, 'UNKNOWN')::TEXT AS symbol_name,
      t.action,
      COALESCE(a.name, 'UNKNOWN')::TEXT AS action_name,
      t.price,
      t.quantity,
      t.confidence,
      t.dry_run
    FROM trades t
    LEFT JOIN enum_symbols s ON t.symbol = s.id
    LEFT JOIN enum_actions a ON t.action = a.id
    WHERE TRUE)sql";
  if (query.symbol_id) {
    sql += std::format(" AND t.symbol = {}", builder.bind(*query.symbol_id));
  }
  if (query.action_id) {
    sql += std::format(" AND t.action = {}", builder.bind(*query.action_id));
  }
  if (query.dry_run) {
    sql += std::format(" AND t.dry_run = {}", builder.bind(*query.dry_run));
  }
  append_time_range_and_pagination(
      sql,
      builder,
      "t.executed_at",
      query.start_time,
      query.end_time,
      "",
      "t.executed_at",
      query.order,
      query.limit,
      query.offset);
  return builder.finish(std::move(sql));
}

built_query build_trading_summary_sql(const summary_query& query) {
  query_builder builder;
  std::string trade_where = "WHERE TRUE";
  std::string candle_where = "WHERE TRUE";
  std::string candle_time_filter;

  if (query.symbol_id) {
    std::string symbol_param = builder.bind(*query.symbol_id);
    trade_where += std::format(" AND symbol = {}", symbol_param);
    candle_where += std::format(" AND symbol = {}", symbol_param);
  }
  if (query.dry_run) {
    std::string dry_run_param = builder.bind(*query.dry_run);
    trade_where += std::format(" AND dry_run = {}", dry_run_param);
  }
  if (query.start_time) {
    std::string start_param = builder.bind(*query.start_time);
    trade_where += std::format(" AND executed_at >= {}", start_param);
    candle_where += std::format(" AND opened_at >= {}", start_param);
    candle_time_filter += std::format(" AND opened_at >= {}", start_param);
  }
  if (query.end_time) {
    std::string end_param = builder.bind(*query.end_time);
    trade_where += std::format(" AND executed_at <= {}", end_param);
    candle_where += std::format(" AND opened_at <= {}", end_param);
    candle_time_filter += std::format(" AND opened_at <= {}", end_param);
  }

  std::string sql = std::format(
      R"sql(
        WITH trade_agg AS (
          SELECT
            symbol,
            COUNT(*)::BIGINT AS total_trades,
            COUNT(*) FILTER (WHERE action = {3})::BIGINT AS buy_count,
            COUNT(*) FILTER (WHERE action = {4})::BIGINT AS sell_count,
            COUNT(*) FILTER (WHERE action = {5})::BIGINT AS hold_count,
            COALESCE(SUM(quantity) FILTER (WHERE action = {3}), 0)::BIGINT
              AS buy_quantity,
            COALESCE(SUM(quantity) FILTER (WHERE action = {4}), 0)::BIGINT
              AS sell_quantity,
            COALESCE(
              SUM(price * quantity) FILTER (WHERE action = {3}), 0
            )::DOUBLE PRECISION AS buy_notional,
            COALESCE(
              SUM(price * quantity) FILTER (WHERE action = {4}), 0
            )::DOUBLE PRECISION AS sell_notional,
            COUNT(*) FILTER (WHERE dry_run)::BIGINT AS dry_run_count,
            COUNT(*) FILTER (WHERE NOT dry_run)::BIGINT AS live_count,
            AVG(confidence)::DOUBLE PRECISION AS avg_confidence,
            MIN(executed_at) AS first_trade_at,
            MAX(executed_at) AS last_trade_at
          FROM trades
          {0}
          GROUP BY symbol
        ),
        candle_agg AS (
          SELECT
            symbol,
            COUNT(*)::BIGINT AS candle_count,
            MAX(high)::DOUBLE PRECISION AS high,
            MIN(low)::DOUBLE PRECISION AS low,
            COALESCE(SUM(volume), 0)::BIGINT AS total_volume
          FROM candles
          {1}
          GROUP BY symbol
        )
        SELECT
          COALESCE(t.symbol, c.symbol) AS symbol_id,
          COALESCE(s.name, 'UNKNOWN')::TEXT AS symbol_name,
          COALESCE(t.total_trades, 0)::BIGINT AS total_trades,
          COALESCE(t.buy_count, 0)::BIGINT AS buy_count,
          COALESCE(t.sell_count, 0)::BIGINT AS sell_count,
          COALESCE(t.hold_count, 0)::BIGINT AS hold_count,
          COALESCE(t.buy_quantity, 0)::BIGINT AS buy_quantity,
          COALESCE(t.sell_quantity, 0)::BIGINT AS sell_quantity,
          COALESCE(t.buy_notional, 0)::DOUBLE PRECISION AS buy_notional,
          COALESCE(t.sell_notional, 0)::DOUBLE PRECISION AS sell_notional,
          COALESCE(t.dry_run_count, 0)::BIGINT AS dry_run_count,
          COALESCE(t.live_count, 0)::BIGINT AS live_count,
          t.avg_confidence,
          t.first_trade_at,
          t.last_trade_at,
          COALESCE(c.candle_count, 0)::BIGINT AS candle_count,
          first_candle.open,
          last_candle.close,
          c.high,
          c.low,
          COALESCE(c.total_volume, 0)::BIGINT AS total_volume
        FROM trade_agg t
        FULL OUTER JOIN candle_agg c ON t.symbol = c.symbol
        LEFT JOIN LATERAL (
          SELECT open::DOUBLE PRECISION AS open
          FROM candles
          WHERE symbol = c.symbol {2}
          ORDER BY opened_at ASC
          LIMIT 1
        ) first_candle ON TRUE
        LEFT JOIN LATERAL (
          SELECT close::DOUBLE PRECISION AS close
          FROM candles
          WHERE symbol = c.symbol {2}
          ORDER BY opened_at DESC
          LIMIT 1
        ) last_candle ON TRUE
        LEFT JOIN enum_symbols s ON s.id = COALESCE(t.symbol, c.symbol)
        ORDER BY symbol_id ASC)sql",
      trade_where,
      candle_where,
      candle_time_filter,
      BUY_ACTION_ID,
      SELL_ACTION_ID,
      HOLD_ACTION_ID);
  return builder.finish(std::move(sql));
}

std::string_view symbols_coverage_sql() {
  return R"sql(
    SELECT
      s.id,
      s.name::TEXT,
      c_min.opened_at,
      c_max.opened_at,
      m_min.emitted_at,
      m_max.emitted_at,
      t_min.executed_at,
      t_max.executed_at,
      COALESCE(tc.cnt, 0)::BIGINT
    FROM enum_symbols s
    LEFT JOIN LATERAL (
      SELECT opened_at FROM candles
      WHERE symbol = s.id ORDER BY opened_at ASC LIMIT 1
    ) c_min ON TRUE
    LEFT JOIN LATERAL (
      SELECT opened_at FROM candles
      WHERE symbol = s.id ORDER BY opened_at DESC LIMIT 1
    ) c_max ON TRUE
    LEFT JOIN LATERAL (
      SELECT emitted_at FROM market
      WHERE symbol = s.id ORDER BY emitted_at ASC LIMIT 1
    ) m_min ON TRUE
    LEFT JOIN LATERAL (
      SELECT emitted_at FROM market
      WHERE symbol = s.id ORDER BY emitted_at DESC LIMIT 1
    ) m_max ON TRUE
    LEFT JOIN LATERAL (
      SELECT executed_at FROM trades
      WHERE symbol = s.id ORDER BY executed_at ASC LIMIT 1
    ) t_min ON TRUE
    LEFT JOIN LATERAL (
      SELECT executed_at FROM trades
      WHERE symbol = s.id ORDER BY executed_at DESC LIMIT 1
    ) t_max ON TRUE
    LEFT JOIN LATERAL (
      SELECT COUNT(*)::BIGINT AS cnt FROM trades WHERE symbol = s.id
    ) tc ON TRUE
    ORDER BY s.id ASC)sql";
}

std::string_view table_estimates_sql() {
  return R"sql(
    SELECT
      relname::TEXT,
      GREATEST(reltuples, 0)::BIGINT
    FROM pg_class
    WHERE relname IN ('candles', 'market', 'trades')
      AND relnamespace = 'public'::regnamespace
    ORDER BY relname ASC)sql";
}

std::string_view actions_metadata_sql() {
  return R"sql(
    SELECT id, name::TEXT
    FROM enum_actions
    ORDER BY id ASC)sql";
}

std::string_view schema_version_sql() {
  return R"sql(
    SELECT v
    FROM howling_version
    LIMIT 1)sql";
}

std::string_view schema_columns_sql() {
  return R"sql(
    SELECT
      table_name::TEXT,
      column_name::TEXT,
      data_type::TEXT,
      (is_nullable = 'YES') AS is_nullable
    FROM information_schema.columns
    WHERE table_schema = 'public'
      AND table_name IN (
        'candles',
        'market',
        'trades',
        'v_trades',
        'enum_symbols',
        'enum_actions',
        'howling_version'
      )
    ORDER BY table_name ASC, ordinal_position ASC)sql";
}

} // namespace howling::reader_internal
