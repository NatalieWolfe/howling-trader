#include "services/db/reader/reader_db.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "services/db/pg/types.h"
#include "services/db/reader/mock_reader_db.h"
#include "services/db/reader/sql_builder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::seconds;
using ::std::chrono::system_clock;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;

constexpr system_clock::time_point BASE_TIME{seconds{1'700'000'000}};

TEST(ReaderSqlBuilderTest, ValidatesPaginationAndBucketSeconds) {
  candle_query bad_limit{.symbol_id = 1, .limit = 0};
  EXPECT_THROW(
      (void)reader_internal::build_candles_sql(bad_limit),
      std::invalid_argument);

  candle_query bad_offset{.symbol_id = 1, .limit = 10, .offset = -1};
  EXPECT_THROW(
      (void)reader_internal::build_candles_sql(bad_offset),
      std::invalid_argument);

  candle_query zero_bucket{
      .symbol_id = 1, .limit = 10, .offset = 0, .bucket_seconds = 0};
  EXPECT_THROW(
      (void)reader_internal::build_candles_sql(zero_bucket),
      std::invalid_argument);

  candle_query negative_bucket{
      .symbol_id = 1, .limit = 10, .offset = 0, .bucket_seconds = -60};
  EXPECT_THROW(
      (void)reader_internal::build_candles_sql(negative_bucket),
      std::invalid_argument);

  candle_query overflow_bucket{
      .symbol_id = 1,
      .limit = 10,
      .offset = 0,
      .bucket_seconds = std::numeric_limits<int64_t>::max() / 1'000'000 + 1};
  EXPECT_THROW(
      (void)reader_internal::build_candles_sql(overflow_bucket),
      std::invalid_argument);

  candle_query max_int64_bucket{
      .symbol_id = 1,
      .limit = 10,
      .offset = 0,
      .bucket_seconds = std::numeric_limits<int64_t>::max()};
  EXPECT_THROW(
      (void)reader_internal::build_candles_sql(max_int64_bucket),
      std::invalid_argument);

  market_query bad_market{.symbol_id = 1, .limit = -5};
  EXPECT_THROW(
      (void)reader_internal::build_market_data_sql(bad_market),
      std::invalid_argument);

  trade_query bad_trade{.limit = 10, .offset = -2};
  EXPECT_THROW(
      (void)reader_internal::build_trades_sql(bad_trade),
      std::invalid_argument);
}

TEST(ReaderSqlBuilderTest, BuildsRawCandlesSqlAndParams) {
  system_clock::time_point start_time = BASE_TIME;
  system_clock::time_point end_time = BASE_TIME + seconds{3600};
  candle_query query{
      .symbol_id = 2,
      .start_time = start_time,
      .end_time = end_time,
      .order = sort_order::ASC,
      .limit = 50,
      .offset = 10};
  reader_internal::built_query built =
      reader_internal::build_candles_sql(query);
  EXPECT_THAT(built.sql, HasSubstr("WHERE symbol = $1"));
  EXPECT_THAT(built.sql, HasSubstr("AND opened_at >= $2"));
  EXPECT_THAT(built.sql, HasSubstr("AND opened_at <= $3"));
  EXPECT_THAT(
      built.sql, HasSubstr("ORDER BY opened_at ASC LIMIT $4 OFFSET $5"));

  ASSERT_EQ(built.params.count(), 5);
  EXPECT_EQ(built.params.types()[0], pg::INT4_OID);
  EXPECT_EQ(
      pg::decode_int32(
          built.params.values()[0], built.params.lengths()[0], pg::INT4_OID),
      2);
  EXPECT_EQ(built.params.types()[1], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          built.params.values()[1],
          built.params.lengths()[1],
          pg::TIMESTAMP_OID),
      start_time);
  EXPECT_EQ(built.params.types()[2], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          built.params.values()[2],
          built.params.lengths()[2],
          pg::TIMESTAMP_OID),
      end_time);
  EXPECT_EQ(built.params.types()[3], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[3], built.params.lengths()[3], pg::INT8_OID),
      51);
  EXPECT_EQ(built.params.types()[4], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[4], built.params.lengths()[4], pg::INT8_OID),
      10);
}

TEST(ReaderSqlBuilderTest, BuildsBucketedCandlesSqlAndParams) {
  candle_query query{
      .symbol_id = 2,
      .order = sort_order::DESC,
      .limit = 20,
      .offset = 0,
      .bucket_seconds = 300};
  reader_internal::built_query built =
      reader_internal::build_candles_sql(query);
  EXPECT_THAT(built.sql, HasSubstr("date_bin("));
  EXPECT_THAT(built.sql, HasSubstr("ARRAY_AGG(open ORDER BY opened_at ASC)"));
  EXPECT_THAT(built.sql, HasSubstr("ARRAY_AGG(close ORDER BY opened_at DESC)"));
  EXPECT_THAT(built.sql, HasSubstr("COALESCE(SUM(volume), 0)::BIGINT"));
  EXPECT_THAT(built.sql, HasSubstr("WHERE symbol = $2"));
  EXPECT_THAT(
      built.sql,
      HasSubstr(
          "GROUP BY symbol, bucket_at ORDER BY bucket_at DESC LIMIT $3 "
          "OFFSET $4"));

  ASSERT_EQ(built.params.count(), 4);
  EXPECT_EQ(built.params.types()[0], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[0], built.params.lengths()[0], pg::INT8_OID),
      300);
  EXPECT_EQ(built.params.types()[1], pg::INT4_OID);
  EXPECT_EQ(
      pg::decode_int32(
          built.params.values()[1], built.params.lengths()[1], pg::INT4_OID),
      2);
  EXPECT_EQ(built.params.types()[2], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[2], built.params.lengths()[2], pg::INT8_OID),
      21);
  EXPECT_EQ(built.params.types()[3], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[3], built.params.lengths()[3], pg::INT8_OID),
      0);
}

TEST(ReaderSqlBuilderTest, BuildsMarketDataSqlAndParams) {
  system_clock::time_point start_time = BASE_TIME + seconds{60};
  market_query query{
      .symbol_id = 1,
      .start_time = start_time,
      .order = sort_order::DESC,
      .limit = 100,
      .offset = 5};
  reader_internal::built_query built =
      reader_internal::build_market_data_sql(query);
  EXPECT_THAT(built.sql, HasSubstr("FROM market"));
  EXPECT_THAT(built.sql, HasSubstr("WHERE symbol = $1"));
  EXPECT_THAT(built.sql, HasSubstr("AND emitted_at >= $2"));
  EXPECT_THAT(
      built.sql, HasSubstr("ORDER BY emitted_at DESC LIMIT $3 OFFSET $4"));

  ASSERT_EQ(built.params.count(), 4);
  EXPECT_EQ(built.params.types()[0], pg::INT4_OID);
  EXPECT_EQ(
      pg::decode_int32(
          built.params.values()[0], built.params.lengths()[0], pg::INT4_OID),
      1);
  EXPECT_EQ(built.params.types()[1], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          built.params.values()[1],
          built.params.lengths()[1],
          pg::TIMESTAMP_OID),
      start_time);
  EXPECT_EQ(built.params.types()[2], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[2], built.params.lengths()[2], pg::INT8_OID),
      101);
  EXPECT_EQ(built.params.types()[3], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[3], built.params.lengths()[3], pg::INT8_OID),
      5);
}

TEST(ReaderSqlBuilderTest, BuildsIndexFriendlyTradesSqlAndParams) {
  system_clock::time_point start_time = BASE_TIME;
  system_clock::time_point end_time = BASE_TIME + seconds{120};
  trade_query query{
      .symbol_id = 1,
      .action_id = 2,
      .dry_run = false,
      .start_time = start_time,
      .end_time = end_time,
      .order = sort_order::DESC,
      .limit = 25,
      .offset = 3};
  reader_internal::built_query built = reader_internal::build_trades_sql(query);
  EXPECT_THAT(built.sql, HasSubstr("FROM trades t"));
  EXPECT_THAT(built.sql, Not(HasSubstr("v_trades")));
  EXPECT_THAT(built.sql, HasSubstr("AND t.symbol = $1"));
  EXPECT_THAT(built.sql, HasSubstr("AND t.action = $2"));
  EXPECT_THAT(built.sql, HasSubstr("AND t.dry_run = $3"));
  EXPECT_THAT(built.sql, HasSubstr("AND t.executed_at >= $4"));
  EXPECT_THAT(built.sql, HasSubstr("AND t.executed_at <= $5"));
  EXPECT_THAT(
      built.sql, HasSubstr("ORDER BY t.executed_at DESC LIMIT $6 OFFSET $7"));

  ASSERT_EQ(built.params.count(), 7);
  EXPECT_EQ(built.params.types()[0], pg::INT4_OID);
  EXPECT_EQ(
      pg::decode_int32(
          built.params.values()[0], built.params.lengths()[0], pg::INT4_OID),
      1);
  EXPECT_EQ(built.params.types()[1], pg::INT4_OID);
  EXPECT_EQ(
      pg::decode_int32(
          built.params.values()[1], built.params.lengths()[1], pg::INT4_OID),
      2);
  EXPECT_EQ(built.params.types()[2], pg::BOOL_OID);
  EXPECT_FALSE(
      pg::decode_bool(
          built.params.values()[2], built.params.lengths()[2], pg::BOOL_OID));
  EXPECT_EQ(built.params.types()[3], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          built.params.values()[3],
          built.params.lengths()[3],
          pg::TIMESTAMP_OID),
      start_time);
  EXPECT_EQ(built.params.types()[4], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          built.params.values()[4],
          built.params.lengths()[4],
          pg::TIMESTAMP_OID),
      end_time);
  EXPECT_EQ(built.params.types()[5], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[5], built.params.lengths()[5], pg::INT8_OID),
      26);
  EXPECT_EQ(built.params.types()[6], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[6], built.params.lengths()[6], pg::INT8_OID),
      3);
}

TEST(ReaderSqlBuilderTest, BuildsTradesSqlWithNoFilters) {
  trade_query query{.order = sort_order::ASC, .limit = 15, .offset = 0};
  reader_internal::built_query built = reader_internal::build_trades_sql(query);
  EXPECT_THAT(built.sql, Not(HasSubstr("AND t.symbol")));
  EXPECT_THAT(built.sql, Not(HasSubstr("AND t.action")));
  EXPECT_THAT(built.sql, Not(HasSubstr("AND t.dry_run")));
  EXPECT_THAT(built.sql, Not(HasSubstr("AND t.executed_at")));
  EXPECT_THAT(
      built.sql, HasSubstr("ORDER BY t.executed_at ASC LIMIT $1 OFFSET $2"));

  ASSERT_EQ(built.params.count(), 2);
  EXPECT_EQ(built.params.types()[0], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[0], built.params.lengths()[0], pg::INT8_OID),
      16);
  EXPECT_EQ(built.params.types()[1], pg::INT8_OID);
  EXPECT_EQ(
      pg::decode_int64(
          built.params.values()[1], built.params.lengths()[1], pg::INT8_OID),
      0);
}

TEST(ReaderSqlBuilderTest, BuildsTradingSummaryWithOuterJoinLateralAndCasts) {
  system_clock::time_point start_time = BASE_TIME;
  system_clock::time_point end_time = BASE_TIME + seconds{600};
  summary_query query{
      .symbol_id = 1,
      .dry_run = true,
      .start_time = start_time,
      .end_time = end_time};
  reader_internal::built_query built =
      reader_internal::build_trading_summary_sql(query);
  EXPECT_THAT(built.sql, HasSubstr("FULL OUTER JOIN candle_agg"));
  EXPECT_THAT(built.sql, HasSubstr("LEFT JOIN LATERAL"));
  EXPECT_THAT(built.sql, Not(HasSubstr("ARRAY_AGG")));
  EXPECT_THAT(built.sql, HasSubstr("buy_notional"));
  EXPECT_THAT(built.sql, HasSubstr("sell_notional"));
  EXPECT_THAT(built.sql, HasSubstr("::DOUBLE PRECISION"));
  EXPECT_THAT(built.sql, HasSubstr("::BIGINT"));
  EXPECT_THAT(built.sql, HasSubstr("AND symbol = $1"));
  EXPECT_THAT(built.sql, HasSubstr("AND dry_run = $2"));
  EXPECT_THAT(built.sql, HasSubstr("AND executed_at >= $3"));
  EXPECT_THAT(built.sql, HasSubstr("AND opened_at >= $3"));
  EXPECT_THAT(built.sql, HasSubstr("AND executed_at <= $4"));
  EXPECT_THAT(built.sql, HasSubstr("AND opened_at <= $4"));

  ASSERT_EQ(built.params.count(), 4);
  EXPECT_EQ(built.params.types()[0], pg::INT4_OID);
  EXPECT_EQ(
      pg::decode_int32(
          built.params.values()[0], built.params.lengths()[0], pg::INT4_OID),
      1);
  EXPECT_EQ(built.params.types()[1], pg::BOOL_OID);
  EXPECT_TRUE(
      pg::decode_bool(
          built.params.values()[1], built.params.lengths()[1], pg::BOOL_OID));
  EXPECT_EQ(built.params.types()[2], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          built.params.values()[2],
          built.params.lengths()[2],
          pg::TIMESTAMP_OID),
      start_time);
  EXPECT_EQ(built.params.types()[3], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          built.params.values()[3],
          built.params.lengths()[3],
          pg::TIMESTAMP_OID),
      end_time);
}

TEST(ReaderSqlBuilderTest, BuildsTradingSummaryWithNoAndPartialFilters) {
  summary_query empty_query{};
  reader_internal::built_query empty_built =
      reader_internal::build_trading_summary_sql(empty_query);
  EXPECT_EQ(empty_built.params.count(), 0);
  EXPECT_THAT(empty_built.sql, Not(HasSubstr("$1")));

  summary_query partial_query{
      .dry_run = false,
      .start_time = BASE_TIME,
  };
  reader_internal::built_query partial_built =
      reader_internal::build_trading_summary_sql(partial_query);
  EXPECT_THAT(partial_built.sql, HasSubstr("AND dry_run = $1"));
  EXPECT_THAT(partial_built.sql, HasSubstr("AND executed_at >= $2"));
  EXPECT_THAT(partial_built.sql, HasSubstr("AND opened_at >= $2"));
  EXPECT_THAT(partial_built.sql, Not(HasSubstr("AND symbol =")));
  EXPECT_THAT(partial_built.sql, Not(HasSubstr("<= $")));

  ASSERT_EQ(partial_built.params.count(), 2);
  EXPECT_EQ(partial_built.params.types()[0], pg::BOOL_OID);
  EXPECT_FALSE(
      pg::decode_bool(
          partial_built.params.values()[0],
          partial_built.params.lengths()[0],
          pg::BOOL_OID));
  EXPECT_EQ(partial_built.params.types()[1], pg::TIMESTAMP_OID);
  EXPECT_EQ(
      pg::decode_timestamp(
          partial_built.params.values()[1],
          partial_built.params.lengths()[1],
          pg::TIMESTAMP_OID),
      BASE_TIME);
}

TEST(ReaderSqlBuilderTest, MetadataQueriesUseLateralReltuplesAndAllowlist) {
  EXPECT_THAT(reader_internal::symbols_coverage_sql(), HasSubstr("LATERAL"));
  EXPECT_THAT(reader_internal::table_estimates_sql(), HasSubstr("pg_class"));
  EXPECT_THAT(reader_internal::table_estimates_sql(), HasSubstr("reltuples"));

  std::string_view schema_sql = reader_internal::schema_columns_sql();
  EXPECT_THAT(schema_sql, HasSubstr("'candles'"));
  EXPECT_THAT(schema_sql, HasSubstr("'market'"));
  EXPECT_THAT(schema_sql, HasSubstr("'trades'"));
  EXPECT_THAT(schema_sql, HasSubstr("'v_trades'"));
  EXPECT_THAT(schema_sql, HasSubstr("'enum_symbols'"));
  EXPECT_THAT(schema_sql, HasSubstr("'enum_actions'"));
  EXPECT_THAT(schema_sql, HasSubstr("'howling_version'"));
  EXPECT_THAT(schema_sql, Not(HasSubstr("auth_tokens")));
}

TEST(ReaderDbPoolTest, AcquiresResetsAndReusesHealthyConnections) {
  int created_count = 0;
  int reset_count = 0;
  reader_db_pool pool(
      [&]() -> std::unique_ptr<reader_db> {
        ++created_count;
        auto mock = std::make_unique<mock_reader_db>();
        ON_CALL(*mock, is_healthy()).WillByDefault(Return(true));
        ON_CALL(*mock, reset()).WillByDefault([&]() { ++reset_count; });
        return mock;
      },
      reader_db_pool::options{.min_size = 0, .max_size = 2});

  {
    reader_db_pool::scoped_resource conn = pool.acquire();
    ASSERT_TRUE(static_cast<bool>(conn));
    EXPECT_TRUE(conn->is_healthy());
  }
  EXPECT_EQ(created_count, 1);
  EXPECT_EQ(reset_count, 1);

  {
    reader_db_pool::scoped_resource conn = pool.acquire();
    ASSERT_TRUE(static_cast<bool>(conn));
  }
  EXPECT_EQ(created_count, 1);
  EXPECT_EQ(reset_count, 2);
}

TEST(ReaderDbPoolTest, DiscardsUnhealthyConnectionsOnRelease) {
  int created_count = 0;
  bool first_healthy = true;
  reader_db_pool pool(
      [&]() -> std::unique_ptr<reader_db> {
        int instance_id = ++created_count;
        auto mock = std::make_unique<mock_reader_db>();
        ON_CALL(*mock, is_healthy()).WillByDefault([&, instance_id]() {
          return instance_id == 1 ? first_healthy : true;
        });
        return mock;
      },
      reader_db_pool::options{.min_size = 0, .max_size = 1});

  {
    reader_db_pool::scoped_resource conn = pool.acquire();
    ASSERT_TRUE(static_cast<bool>(conn));
    // Simulate connection breaking before return to pool.
    first_healthy = false;
  }
  EXPECT_EQ(created_count, 1);
  EXPECT_EQ(pool.pool().available(), 0u);
  EXPECT_EQ(pool.pool().size(), 0u);

  {
    reader_db_pool::scoped_resource conn = pool.acquire();
    ASSERT_TRUE(static_cast<bool>(conn));
    EXPECT_TRUE(conn->is_healthy());
  }
  EXPECT_EQ(created_count, 2);
  EXPECT_EQ(pool.pool().available(), 1u);
}

TEST(ReaderDbPoolTest, DiscardsConnectionsWhoseResetThrows) {
  int created_count = 0;
  reader_db_pool pool(
      [&]() -> std::unique_ptr<reader_db> {
        int instance_id = ++created_count;
        auto mock = std::make_unique<mock_reader_db>();
        ON_CALL(*mock, is_healthy()).WillByDefault(Return(true));
        ON_CALL(*mock, reset()).WillByDefault([instance_id]() {
          if (instance_id == 1) {
            throw std::runtime_error("ROLLBACK failed on poisoned connection.");
          }
        });
        return mock;
      },
      reader_db_pool::options{.min_size = 0, .max_size = 1});

  {
    reader_db_pool::scoped_resource conn = pool.acquire();
    ASSERT_TRUE(static_cast<bool>(conn));
  }
  EXPECT_EQ(created_count, 1);
  EXPECT_EQ(pool.pool().available(), 0u);
  EXPECT_EQ(pool.pool().size(), 0u);

  {
    reader_db_pool::scoped_resource conn = pool.acquire();
    ASSERT_TRUE(static_cast<bool>(conn));
  }
  EXPECT_EQ(created_count, 2);
  EXPECT_EQ(pool.pool().available(), 1u);
}

} // namespace
} // namespace howling
