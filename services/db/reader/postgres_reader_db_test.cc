#include "services/db/reader/postgres_reader_db.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "libpq/libpq-fe.h"
#include "services/db/environment.h"
#include "services/db/pg/connection.h"
#include "services/db/pg/query.h"
#include "services/db/postgres_database.h"
#include "services/db/schema/schema.h"
#include "services/mock_security.h"
#include "time/conversion.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {

struct postgres_reader_db_test_peer {
  static PGconn& connection(postgres_reader_db& reader) {
    return reader._connection();
  }
};

namespace {

using ::std::chrono::microseconds;
using ::std::chrono::seconds;
using ::std::chrono::system_clock;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::Return;
using ::testing::ThrowsMessage;

// 1700000100 is evenly divisible by 300 (5-minute UTC boundary).
constexpr system_clock::time_point BASE_TIME{seconds{1'700'000'100}};

postgres_options test_postgres_options() {
  return postgres_options{
      .host = absl::GetFlag(FLAGS_pg_host),
      .port = std::to_string(absl::GetFlag(FLAGS_pg_port)),
      .user = absl::GetFlag(FLAGS_pg_user),
      .password = absl::GetFlag(FLAGS_pg_password),
      .dbname = absl::GetFlag(FLAGS_pg_database),
      .sslmode = "disable"};
}

Candle make_candle(
    system_clock::time_point opened_at,
    double open,
    double close,
    double high,
    double low,
    int64_t volume,
    int64_t duration_seconds = 60) {
  Candle candle;
  *candle.mutable_opened_at() = to_proto(opened_at);
  *candle.mutable_duration() = to_proto(seconds{duration_seconds});
  candle.set_open(open);
  candle.set_close(close);
  candle.set_high(high);
  candle.set_low(low);
  candle.set_volume(volume);
  return candle;
}

Market make_market(
    stock::Symbol symbol,
    system_clock::time_point emitted_at,
    double bid,
    int64_t bid_lots,
    double ask,
    int64_t ask_lots,
    double last,
    int64_t last_lots) {
  Market market;
  market.set_symbol(symbol);
  *market.mutable_emitted_at() = to_proto(emitted_at);
  market.set_bid(bid);
  market.set_bid_lots(bid_lots);
  market.set_ask(ask);
  market.set_ask_lots(ask_lots);
  market.set_last(last);
  market.set_last_lots(last_lots);
  return market;
}

trading::TradeRecord make_trade(
    stock::Symbol symbol,
    system_clock::time_point executed_at,
    trading::Action action,
    double price,
    int64_t quantity,
    double confidence,
    bool dry_run) {
  trading::TradeRecord trade;
  trade.set_symbol(symbol);
  *trade.mutable_executed_at() = to_proto(executed_at);
  trade.set_action(action);
  trade.set_price(price);
  trade.set_quantity(quantity);
  trade.set_confidence(confidence);
  trade.set_dry_run(dry_run);
  return trade;
}

class PostgresReaderDbTest : public ::testing::Test {
protected:
  void SetUp() override {
    postgres_options options = test_postgres_options();
    _writer = std::make_unique<postgres_database>(_mock_security, options);
    _writer->upgrade_schema("").get();

    pg::unique_conn admin_conn = pg::connect(options);
    pg::execute(
        *admin_conn, R"sql(TRUNCATE candles, market, trades, auth_tokens)sql");

    _reader = std::make_unique<postgres_reader_db>(options);
  }

  mock_security_client _mock_security;
  std::unique_ptr<postgres_database> _writer;
  std::unique_ptr<postgres_reader_db> _reader;
};

TEST_F(
    PostgresReaderDbTest,
    EnforcesReadOnlyAndStatementTimeoutAndResetsTransactions) {
  ASSERT_TRUE(_reader->is_healthy());
  PGconn& reader_conn = postgres_reader_db_test_peer::connection(*_reader);

  std::string read_only_setting;
  pg::execute_read(
      reader_conn,
      R"sql(SHOW default_transaction_read_only)sql",
      read_only_setting);
  EXPECT_EQ(read_only_setting, "on");

  std::string timeout_setting;
  pg::execute_read(
      reader_conn, R"sql(SHOW statement_timeout)sql", timeout_setting);
  EXPECT_EQ(timeout_setting, "10s");

  // Writes must be rejected by PostgreSQL on the read-only connection.
  EXPECT_THAT(
      [&]() {
        pg::execute(
            reader_conn,
            R"sql(
              INSERT INTO candles (
                symbol, open, close, high, low, volume, opened_at, duration_us
              ) VALUES (1, 1.0, 2.0, 3.0, 0.5, 10, TIMESTAMP '2026-01-01 00:00:00', 60000000)
            )sql");
      },
      ThrowsMessage<std::runtime_error>(HasSubstr("read-only transaction")));

  // Simulate a failed query inside an open transaction, then verify reset()
  // rolls it back cleanly so subsequent reads succeed.
  pg::execute(reader_conn, R"sql(BEGIN)sql");
  PGresult* failed_result = PQexec(&reader_conn, "SELECT 1 / 0");
  ASSERT_NE(failed_result, nullptr);
  EXPECT_EQ(PQresultStatus(failed_result), PGRES_FATAL_ERROR);
  PQclear(failed_result);
  EXPECT_EQ(PQtransactionStatus(&reader_conn), PQTRANS_INERROR);

  _reader->reset();
  EXPECT_EQ(PQtransactionStatus(&reader_conn), PQTRANS_IDLE);
  EXPECT_FALSE(_reader->read_actions_metadata().empty());
}

TEST_F(PostgresReaderDbTest, ReadsRawAndBucketedCandlesWithPagination) {
  _writer
      ->save(
          stock::NVDA,
          make_candle(BASE_TIME + seconds{60}, 10.0, 11.0, 12.0, 9.5, 100))
      .get();
  _writer
      ->save(
          stock::NVDA,
          make_candle(BASE_TIME + seconds{180}, 11.0, 13.0, 14.0, 10.5, 250))
      .get();
  _writer
      ->save(
          stock::NVDA,
          make_candle(BASE_TIME + seconds{360}, 13.0, 12.5, 15.0, 12.0, 300))
      .get();
  _writer
      ->save(
          stock::NVDA,
          make_candle(BASE_TIME + seconds{480}, 12.5, 16.0, 16.5, 12.2, 400))
      .get();
  _writer
      ->save(
          stock::AAPL,
          make_candle(BASE_TIME + seconds{60}, 200.0, 201.0, 202.0, 199.0, 50))
      .get();

  // Raw candles: ASC pagination with has_more.
  candle_page first_page = _reader->read_candles(
      candle_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .order = sort_order::ASC,
          .limit = 2,
          .offset = 0});
  ASSERT_EQ(first_page.rows.size(), 2u);
  EXPECT_TRUE(first_page.has_more);
  EXPECT_EQ(first_page.rows[0].opened_at, BASE_TIME + seconds{60});
  EXPECT_DOUBLE_EQ(first_page.rows[0].open, 10.0);
  EXPECT_EQ(first_page.rows[0].duration_us, 60'000'000);
  EXPECT_EQ(first_page.rows[1].opened_at, BASE_TIME + seconds{180});

  candle_page second_page = _reader->read_candles(
      candle_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .order = sort_order::ASC,
          .limit = 2,
          .offset = 2});
  ASSERT_EQ(second_page.rows.size(), 2u);
  EXPECT_FALSE(second_page.has_more);
  EXPECT_EQ(second_page.rows[0].opened_at, BASE_TIME + seconds{360});
  EXPECT_EQ(second_page.rows[1].opened_at, BASE_TIME + seconds{480});

  // Raw candles: inclusive time range in DESC order.
  candle_page filtered_page = _reader->read_candles(
      candle_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .start_time = BASE_TIME + seconds{180},
          .end_time = BASE_TIME + seconds{360},
          .order = sort_order::DESC,
          .limit = 10,
          .offset = 0});
  ASSERT_EQ(filtered_page.rows.size(), 2u);
  EXPECT_FALSE(filtered_page.has_more);
  EXPECT_EQ(filtered_page.rows[0].opened_at, BASE_TIME + seconds{360});
  EXPECT_EQ(filtered_page.rows[1].opened_at, BASE_TIME + seconds{180});

  // Bucketed candles (5-minute buckets via date_bin) with has_more pagination.
  candle_page bucket_page_one = _reader->read_candles(
      candle_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .order = sort_order::ASC,
          .limit = 1,
          .offset = 0,
          .bucket_seconds = 300});
  ASSERT_EQ(bucket_page_one.rows.size(), 1u);
  EXPECT_TRUE(bucket_page_one.has_more);
  const candle_row& first_bucket = bucket_page_one.rows[0];
  EXPECT_EQ(first_bucket.symbol_id, static_cast<int32_t>(stock::NVDA));
  EXPECT_EQ(first_bucket.opened_at, BASE_TIME);
  EXPECT_DOUBLE_EQ(first_bucket.open, 10.0);
  EXPECT_DOUBLE_EQ(first_bucket.close, 13.0);
  EXPECT_DOUBLE_EQ(first_bucket.high, 14.0);
  EXPECT_DOUBLE_EQ(first_bucket.low, 9.5);
  EXPECT_EQ(first_bucket.volume, 350);
  EXPECT_EQ(first_bucket.duration_us, 300'000'000);

  candle_page bucket_page_two = _reader->read_candles(
      candle_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .order = sort_order::ASC,
          .limit = 1,
          .offset = 1,
          .bucket_seconds = 300});
  ASSERT_EQ(bucket_page_two.rows.size(), 1u);
  EXPECT_FALSE(bucket_page_two.has_more);
  const candle_row& second_bucket = bucket_page_two.rows[0];
  EXPECT_EQ(second_bucket.opened_at, BASE_TIME + seconds{300});
  EXPECT_DOUBLE_EQ(second_bucket.open, 13.0);
  EXPECT_DOUBLE_EQ(second_bucket.close, 16.0);
  EXPECT_DOUBLE_EQ(second_bucket.high, 16.5);
  EXPECT_DOUBLE_EQ(second_bucket.low, 12.0);
  EXPECT_EQ(second_bucket.volume, 700);
  EXPECT_EQ(second_bucket.duration_us, 300'000'000);
}

TEST_F(PostgresReaderDbTest, ReadsMarketDataWithFilteringAndPagination) {
  _writer
      ->save(make_market(
          stock::NVDA, BASE_TIME + seconds{10}, 100.0, 5, 100.5, 6, 100.2, 7))
      .get();
  _writer
      ->save(make_market(
          stock::NVDA, BASE_TIME + seconds{20}, 101.0, 8, 101.5, 9, 101.2, 10))
      .get();
  _writer
      ->save(make_market(
          stock::NVDA,
          BASE_TIME + seconds{30},
          102.0,
          11,
          102.5,
          12,
          102.2,
          13))
      .get();
  _writer
      ->save(make_market(
          stock::AAPL, BASE_TIME + seconds{15}, 200.0, 1, 200.5, 2, 200.2, 3))
      .get();

  market_page first_page = _reader->read_market_data(
      market_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .order = sort_order::DESC,
          .limit = 2,
          .offset = 0});
  ASSERT_EQ(first_page.rows.size(), 2u);
  EXPECT_TRUE(first_page.has_more);
  EXPECT_EQ(first_page.rows[0].emitted_at, BASE_TIME + seconds{30});
  EXPECT_DOUBLE_EQ(first_page.rows[0].bid, 102.0);
  EXPECT_EQ(first_page.rows[0].bid_lots, 11);
  EXPECT_DOUBLE_EQ(first_page.rows[0].ask, 102.5);
  EXPECT_EQ(first_page.rows[0].ask_lots, 12);
  EXPECT_DOUBLE_EQ(first_page.rows[0].last, 102.2);
  EXPECT_EQ(first_page.rows[0].last_lots, 13);
  EXPECT_EQ(first_page.rows[1].emitted_at, BASE_TIME + seconds{20});

  market_page second_page = _reader->read_market_data(
      market_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .order = sort_order::DESC,
          .limit = 2,
          .offset = 2});
  ASSERT_EQ(second_page.rows.size(), 1u);
  EXPECT_FALSE(second_page.has_more);
  EXPECT_EQ(second_page.rows[0].emitted_at, BASE_TIME + seconds{10});
}

TEST_F(PostgresReaderDbTest, ReadsTradesWithFiltersAndPagination) {
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{60},
          trading::BUY,
          100.0,
          10,
          0.9,
          false))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{120},
          trading::SELL,
          110.0,
          5,
          0.7,
          true))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{180},
          trading::HOLD,
          105.0,
          0,
          0.5,
          false))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::AAPL,
          BASE_TIME + seconds{240},
          trading::BUY,
          200.0,
          2,
          0.8,
          false))
      .get();

  trade_page nvda_live = _reader->read_trades(
      trade_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .dry_run = false,
          .order = sort_order::ASC,
          .limit = 1,
          .offset = 0});
  ASSERT_EQ(nvda_live.rows.size(), 1u);
  EXPECT_TRUE(nvda_live.has_more);
  EXPECT_EQ(nvda_live.rows[0].symbol_name, "NVDA");
  EXPECT_EQ(nvda_live.rows[0].action_name, "BUY");
  EXPECT_DOUBLE_EQ(nvda_live.rows[0].price, 100.0);
  EXPECT_EQ(nvda_live.rows[0].quantity, 10);
  EXPECT_FALSE(nvda_live.rows[0].dry_run);

  trade_page dry_run_sells = _reader->read_trades(
      trade_query{
          .action_id = static_cast<int32_t>(trading::SELL),
          .dry_run = true,
          .order = sort_order::DESC,
          .limit = 10,
          .offset = 0});
  ASSERT_EQ(dry_run_sells.rows.size(), 1u);
  EXPECT_FALSE(dry_run_sells.has_more);
  EXPECT_EQ(dry_run_sells.rows[0].symbol_name, "NVDA");
  EXPECT_EQ(dry_run_sells.rows[0].action_name, "SELL");
  EXPECT_TRUE(dry_run_sells.rows[0].dry_run);
}

TEST_F(
    PostgresReaderDbTest,
    ReadsTradingSummaryIncludingCandleOnlyAndTradeOnlySymbols) {
  // NVDA: both candles and trades.
  _writer
      ->save(
          stock::NVDA,
          make_candle(BASE_TIME + seconds{60}, 95.0, 100.0, 102.0, 94.0, 1000))
      .get();
  _writer
      ->save(
          stock::NVDA,
          make_candle(
              BASE_TIME + seconds{120}, 100.0, 115.0, 120.0, 99.0, 1500))
      .get();
  _writer
      ->save(
          stock::NVDA,
          make_candle(
              BASE_TIME + seconds{180}, 115.0, 112.0, 118.0, 110.0, 500))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{70},
          trading::BUY,
          100.0,
          10,
          0.8,
          false))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{130},
          trading::SELL,
          120.0,
          5,
          0.6,
          true))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{190},
          trading::HOLD,
          110.0,
          0,
          0.4,
          false))
      .get();

  // AAPL: candles only, zero trades.
  _writer
      ->save(
          stock::AAPL,
          make_candle(BASE_TIME + seconds{60}, 200.0, 205.0, 206.0, 198.0, 400))
      .get();
  _writer
      ->save(
          stock::AAPL,
          make_candle(
              BASE_TIME + seconds{120}, 205.0, 210.0, 212.0, 204.0, 600))
      .get();

  // MU: trades only, zero candles.
  _writer
      ->save_trade(make_trade(
          stock::MU,
          BASE_TIME + seconds{90},
          trading::BUY,
          50.0,
          20,
          0.9,
          false))
      .get();

  std::vector<trading_summary_row> summary =
      _reader->read_trading_summary(summary_query{});
  ASSERT_EQ(summary.size(), 3u);

  // NVDA (id 1)
  const trading_summary_row& nvda = summary[0];
  EXPECT_EQ(nvda.symbol_id, static_cast<int32_t>(stock::NVDA));
  EXPECT_EQ(nvda.symbol_name, "NVDA");
  EXPECT_EQ(nvda.total_trades, 3);
  EXPECT_EQ(nvda.buy_count, 1);
  EXPECT_EQ(nvda.sell_count, 1);
  EXPECT_EQ(nvda.hold_count, 1);
  EXPECT_EQ(nvda.buy_quantity, 10);
  EXPECT_EQ(nvda.sell_quantity, 5);
  EXPECT_DOUBLE_EQ(nvda.buy_notional, 1000.0);
  EXPECT_DOUBLE_EQ(nvda.sell_notional, 600.0);
  EXPECT_EQ(nvda.dry_run_count, 1);
  EXPECT_EQ(nvda.live_count, 2);
  ASSERT_TRUE(nvda.avg_confidence.has_value());
  EXPECT_NEAR(*nvda.avg_confidence, 0.6, 1e-9);
  EXPECT_EQ(nvda.first_trade_at, BASE_TIME + seconds{70});
  EXPECT_EQ(nvda.last_trade_at, BASE_TIME + seconds{190});
  EXPECT_EQ(nvda.candle_count, 3);
  EXPECT_EQ(nvda.open, 95.0);
  EXPECT_EQ(nvda.close, 112.0);
  EXPECT_EQ(nvda.high, 120.0);
  EXPECT_EQ(nvda.low, 94.0);
  EXPECT_EQ(nvda.total_volume, 3000);

  // AAPL (id 2): candles with zero trades must still appear.
  const trading_summary_row& aapl = summary[1];
  EXPECT_EQ(aapl.symbol_id, static_cast<int32_t>(stock::AAPL));
  EXPECT_EQ(aapl.symbol_name, "AAPL");
  EXPECT_EQ(aapl.total_trades, 0);
  EXPECT_DOUBLE_EQ(aapl.buy_notional, 0.0);
  EXPECT_DOUBLE_EQ(aapl.sell_notional, 0.0);
  EXPECT_FALSE(aapl.avg_confidence.has_value());
  EXPECT_FALSE(aapl.first_trade_at.has_value());
  EXPECT_FALSE(aapl.last_trade_at.has_value());
  EXPECT_EQ(aapl.candle_count, 2);
  EXPECT_EQ(aapl.open, 200.0);
  EXPECT_EQ(aapl.close, 210.0);
  EXPECT_EQ(aapl.high, 212.0);
  EXPECT_EQ(aapl.low, 198.0);
  EXPECT_EQ(aapl.total_volume, 1000);

  // MU (id 3): trades with zero candles.
  const trading_summary_row& mu = summary[2];
  EXPECT_EQ(mu.symbol_id, static_cast<int32_t>(stock::MU));
  EXPECT_EQ(mu.symbol_name, "MU");
  EXPECT_EQ(mu.total_trades, 1);
  EXPECT_DOUBLE_EQ(mu.buy_notional, 1000.0);
  EXPECT_EQ(mu.candle_count, 0);
  EXPECT_FALSE(mu.open.has_value());
  EXPECT_FALSE(mu.close.has_value());
  EXPECT_FALSE(mu.high.has_value());
  EXPECT_FALSE(mu.low.has_value());
  EXPECT_EQ(mu.total_volume, 0);

  // Time-bounded summary verifies LATERAL first/last open/close respect bounds.
  std::vector<trading_summary_row> windowed = _reader->read_trading_summary(
      summary_query{
          .symbol_id = static_cast<int32_t>(stock::NVDA),
          .start_time = BASE_TIME + seconds{100},
          .end_time = BASE_TIME + seconds{150}});
  ASSERT_EQ(windowed.size(), 1u);
  EXPECT_EQ(windowed[0].candle_count, 1);
  EXPECT_EQ(windowed[0].open, 100.0);
  EXPECT_EQ(windowed[0].close, 115.0);
  EXPECT_EQ(windowed[0].total_trades, 1);
  EXPECT_EQ(windowed[0].sell_count, 1);
}

TEST_F(PostgresReaderDbTest, ReadsSymbolsActionsAndAllowlistedSchemaMetadata) {
  _writer
      ->save(
          stock::NVDA,
          make_candle(BASE_TIME + seconds{60}, 10.0, 11.0, 12.0, 9.0, 100))
      .get();
  _writer
      ->save(
          stock::NVDA,
          make_candle(BASE_TIME + seconds{180}, 11.0, 12.0, 13.0, 10.0, 200))
      .get();
  _writer
      ->save(make_market(
          stock::NVDA, BASE_TIME + seconds{30}, 10.0, 1, 10.5, 2, 10.2, 3))
      .get();
  _writer
      ->save(make_market(
          stock::NVDA, BASE_TIME + seconds{90}, 11.0, 1, 11.5, 2, 11.2, 3))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{70},
          trading::BUY,
          10.5,
          4,
          0.85,
          false))
      .get();
  _writer
      ->save_trade(make_trade(
          stock::NVDA,
          BASE_TIME + seconds{140},
          trading::SELL,
          11.5,
          4,
          0.75,
          false))
      .get();

  EXPECT_CALL(_mock_security, encrypt(_, _))
      .WillOnce(Return("vault:v1:secret"));
  _writer->save_refresh_token("schwab", "plaintext-token").get();

  // Populate pg_class.reltuples estimates.
  pg::unique_conn admin_conn = pg::connect(test_postgres_options());
  pg::execute(*admin_conn, R"sql(ANALYZE candles, market, trades)sql");

  symbols_metadata symbols = _reader->read_symbols_metadata();
  std::unordered_map<int32_t, symbol_metadata> by_symbol_id;
  for (const symbol_metadata& entry : symbols.symbols) {
    by_symbol_id.emplace(entry.symbol_id, entry);
  }
  ASSERT_TRUE(by_symbol_id.contains(static_cast<int32_t>(stock::NVDA)));
  const symbol_metadata& nvda =
      by_symbol_id.at(static_cast<int32_t>(stock::NVDA));
  EXPECT_EQ(nvda.symbol_name, "NVDA");
  EXPECT_EQ(nvda.min_candle_at, BASE_TIME + seconds{60});
  EXPECT_EQ(nvda.max_candle_at, BASE_TIME + seconds{180});
  EXPECT_EQ(nvda.min_market_at, BASE_TIME + seconds{30});
  EXPECT_EQ(nvda.max_market_at, BASE_TIME + seconds{90});
  EXPECT_EQ(nvda.min_trade_at, BASE_TIME + seconds{70});
  EXPECT_EQ(nvda.max_trade_at, BASE_TIME + seconds{140});
  EXPECT_EQ(nvda.trade_count, 2);

  ASSERT_TRUE(by_symbol_id.contains(static_cast<int32_t>(stock::RIVN)));
  const symbol_metadata& rivn =
      by_symbol_id.at(static_cast<int32_t>(stock::RIVN));
  EXPECT_FALSE(rivn.min_candle_at.has_value());
  EXPECT_FALSE(rivn.min_market_at.has_value());
  EXPECT_FALSE(rivn.min_trade_at.has_value());
  EXPECT_EQ(rivn.trade_count, 0);

  std::unordered_map<std::string, int64_t> estimates;
  for (const table_row_estimate& estimate : symbols.table_estimates) {
    estimates.emplace(estimate.table_name, estimate.approx_rows);
  }
  EXPECT_EQ(estimates.at("candles"), 2);
  EXPECT_EQ(estimates.at("market"), 2);
  EXPECT_EQ(estimates.at("trades"), 2);

  std::vector<action_info> actions = _reader->read_actions_metadata();
  ASSERT_GE(actions.size(), 4u);
  EXPECT_EQ(actions[0].id, static_cast<int32_t>(trading::ACTION_UNSPECIFIED));
  EXPECT_EQ(actions[1].id, static_cast<int32_t>(trading::HOLD));
  EXPECT_EQ(actions[1].name, "HOLD");
  EXPECT_EQ(actions[2].id, static_cast<int32_t>(trading::BUY));
  EXPECT_EQ(actions[2].name, "BUY");
  EXPECT_EQ(actions[3].id, static_cast<int32_t>(trading::SELL));
  EXPECT_EQ(actions[3].name, "SELL");

  schema_metadata schema = _reader->read_schema_metadata();
  EXPECT_EQ(schema.schema_version, db_internal::get_schema_version());
  std::unordered_set<std::string> table_names;
  for (const table_metadata& table : schema.tables) {
    EXPECT_FALSE(table.columns.empty());
    table_names.insert(table.table_name);
  }
  EXPECT_THAT(
      table_names,
      IsSupersetOf(
          {"candles",
           "market",
           "trades",
           "v_trades",
           "enum_symbols",
           "enum_actions",
           "howling_version"}));
  EXPECT_THAT(table_names, Not(Contains("auth_tokens")));
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);
  return RUN_ALL_TESTS();
}
