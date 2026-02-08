#include "services/db/sqlite_database.h"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/random/random.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "sqlite3.h"
#include "time/conversion.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_DECLARE_FLAG(std::string, sqlite_db_path);

namespace howling {
namespace {

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using ::std::chrono::system_clock;
using ::testing::AllOf;
using ::testing::IsSupersetOf;
using ::testing::Property;

constexpr std::string_view SHARED_MEMORY_DB_PATH = "file::memory:?cache=shared";
constexpr std::string_view MEMORY_DB_PATH = ":memory:";

void set_memory_database_path() {
  absl::SetFlag(&FLAGS_sqlite_db_path, MEMORY_DB_PATH);
}

TEST(SqliteDatabase, InitializesEmptyDatabase) {
  // Use a shared cache memory DB for this test to be able to connect a second
  // handle to check the schema.
  absl::SetFlag(&FLAGS_sqlite_db_path, SHARED_MEMORY_DB_PATH);

  std::unique_ptr<sqlite_database> db;
  EXPECT_NO_THROW(db = std::make_unique<sqlite_database>());

  sqlite3* raw_db;
  ASSERT_EQ(sqlite3_open(SHARED_MEMORY_DB_PATH.data(), &raw_db), SQLITE_OK);

  std::unordered_set<std::string> found_tables;
  ASSERT_EQ(
      sqlite3_exec(
          raw_db,
          "SELECT name FROM sqlite_schema",
          [](void* table_set, int column_count, char** columns, char**) {
            if (column_count != 1) return 1;
            reinterpret_cast<std::unordered_set<std::string>*>(table_set)
                ->insert(columns[0]);
            return 0;
          },
          &found_tables,
          nullptr),
      SQLITE_OK);
  EXPECT_THAT(
      found_tables, IsSupersetOf({"howling_version", "candles", "trades"}));
  sqlite3_close_v2(raw_db);
}

// MARK: Candle

TEST(SqliteCandles, CanSaveCandles) {
  set_memory_database_path();
  sqlite_database db;

  EXPECT_NO_THROW(db.save(stock::NVDA, Candle::default_instance()).get());
}

TEST(SqliteCandles, SavedCandlesAreReadable) {
  set_memory_database_path();
  sqlite_database db;

  Candle candle;
  candle.set_open(1.0);
  candle.set_close(2.0);
  candle.set_high(3.0);
  candle.set_low(4.0);
  candle.set_volume(5);
  candle.mutable_opened_at()->set_seconds(6);
  candle.mutable_duration()->set_seconds(7);

  db.save(stock::NVDA, candle).get();

  int count = 0;
  for (const Candle& found_candle : db.read_candles(stock::NVDA)) {
    ++count;
    EXPECT_THAT(
        found_candle,
        AllOf(
            Property("open", &Candle::open, candle.open()),
            Property("close", &Candle::close, candle.close()),
            Property("high", &Candle::high, candle.high()),
            Property("low", &Candle::low, candle.low()),
            Property("volume", &Candle::volume, candle.volume()),
            Property(
                "opened_at",
                &Candle::opened_at,
                Property(
                    "seconds",
                    &Timestamp::seconds,
                    candle.opened_at().seconds())),
            Property(
                "duration",
                &Candle::duration,
                Property(
                    "seconds",
                    &Duration::seconds,
                    candle.duration().seconds()))));
  }
  EXPECT_EQ(count, 1);
}

TEST(SqliteCandles, ReadingIteratesOverAllCandlesInOrder) {
  set_memory_database_path();
  sqlite_database db;

  constexpr int CANDLE_COUNT = 100;
  absl::BitGen rand_gen;
  for (int i = 0; i < CANDLE_COUNT; ++i) {
    Candle candle;
    candle.set_open(i);
    candle.mutable_opened_at()->set_seconds(
        absl::Uniform(rand_gen, 0ll, 1ll << 30));
    db.save(stock::NVDA, candle).get();
  }

  int counter = 0;
  int64_t prev_opened_at = -1;
  for (const Candle& candle : db.read_candles(stock::NVDA)) {
    ++counter;
    EXPECT_GT(candle.opened_at().seconds(), prev_opened_at);
    prev_opened_at = candle.opened_at().seconds();
  }
  EXPECT_EQ(counter, CANDLE_COUNT);
}

// MARK: Market

TEST(SqliteMarket, CanSaveMarket) {
  set_memory_database_path();
  sqlite_database db;

  EXPECT_NO_THROW(db.save(Market::default_instance()).get());
}

TEST(SqliteMarket, SavedMarketHistoryIsReadable) {
  set_memory_database_path();
  sqlite_database db;

  Market market;
  market.set_symbol(stock::NVDA);
  market.set_bid(1.0);
  market.set_bid_lots(2);
  market.set_ask(3.0);
  market.set_ask_lots(4);
  market.set_last(5.0);
  market.set_last_lots(6);
  market.mutable_emitted_at()->set_seconds(7);

  db.save(market).get();

  int count = 0;
  for (const Market& found_market : db.read_market(stock::NVDA)) {
    ++count;
    EXPECT_THAT(
        found_market,
        AllOf(
            Property("bid", &Market::bid, market.bid()),
            Property("bid_lots", &Market::bid_lots, market.bid_lots()),
            Property("ask", &Market::ask, market.ask()),
            Property("ask_lots", &Market::ask_lots, market.ask_lots()),
            Property("last", &Market::last, market.last()),
            Property("last_lots", &Market::last_lots, market.last_lots()),
            Property(
                "emitted_at",
                &Market::emitted_at,
                Property(
                    "seconds",
                    &Timestamp::seconds,
                    market.emitted_at().seconds()))));
  }
  EXPECT_EQ(count, 1);
}

TEST(SqliteMarket, ReadingIteratesOverAllMarketHistoryInOrder) {
  set_memory_database_path();
  sqlite_database db;

  constexpr int MARKET_COUNT = 100;
  absl::BitGen rand_gen;
  for (int i = 0; i < MARKET_COUNT; ++i) {
    Market market;
    market.set_symbol(stock::NVDA);
    market.set_bid(i);
    market.mutable_emitted_at()->set_seconds(
        absl::Uniform(rand_gen, 0ll, 1ll << 30));
    db.save(market).get();
  }

  int counter = 0;
  int64_t prev_emitted_at = -1;
  for (const Market& market : db.read_market(stock::NVDA)) {
    ++counter;
    EXPECT_GT(market.emitted_at().seconds(), prev_emitted_at);
    prev_emitted_at = market.emitted_at().seconds();
  }
  EXPECT_EQ(counter, MARKET_COUNT);
}

// MARK: Trade

TEST(SqliteTrades, CanSaveTrade) {
  set_memory_database_path();
  sqlite_database db;
  trading::TradeRecord trade;
  trade.set_symbol(stock::NVDA);
  trade.set_action(trading::BUY);
  trade.set_price(100.0);
  trade.set_quantity(10);
  trade.set_confidence(0.9);
  trade.set_dry_run(true);
  *trade.mutable_executed_at() = to_proto(system_clock::now());

  EXPECT_NO_THROW(db.save_trade(trade).get());
}

TEST(SqliteTrades, SavedTradesAreReadable) {
  set_memory_database_path();
  sqlite_database db;
  trading::TradeRecord trade;
  trade.set_symbol(stock::NVDA);
  trade.set_action(trading::BUY);
  trade.set_price(150.0);
  trade.set_quantity(5);
  trade.set_confidence(0.85);
  trade.set_dry_run(true);
  *trade.mutable_executed_at() = to_proto(system_clock::now());
  db.save_trade(trade).get();
  int count = 0;

  for (const trading::TradeRecord& trade : db.read_trades(stock::NVDA)) {
    ++count;
    EXPECT_EQ(trade.symbol(), stock::NVDA);
    EXPECT_EQ(trade.action(), trading::BUY);
    EXPECT_DOUBLE_EQ(trade.price(), 150.0);
    EXPECT_EQ(trade.quantity(), 5);
    EXPECT_DOUBLE_EQ(trade.confidence(), 0.85);
    EXPECT_TRUE(trade.dry_run());
  }

  EXPECT_EQ(count, 1);
}

} // namespace
} // namespace howling
