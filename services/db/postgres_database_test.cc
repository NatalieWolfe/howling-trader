#include "services/db/postgres_database.h"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/random/random.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "libpq/libpq-fe.h"
#include "time/conversion.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_FLAG(
    std::string, pg_user, "", "User to connect as to the Postgres database.");
ABSL_FLAG(
    std::string,
    pg_password,
    "",
    "User password to connect to the Postgres database.");
ABSL_FLAG(std::string, pg_host, "", "Hostname for the Postgres database.");
ABSL_FLAG(int, pg_port, 5432, "Port to connect to for the Postgres database.");
ABSL_FLAG(
    std::string, pg_database, "howling", "Name of the Postgres database.");

namespace howling {
namespace {

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using ::std::chrono::system_clock;
using ::testing::AllOf;
using ::testing::Property;

class PostgresDatabaseTest : public ::testing::Test {
protected:
  void SetUp() override {
    _port_str = std::to_string(absl::GetFlag(FLAGS_pg_port));
    _db = std::make_unique<postgres_database>(postgres_options{
        .host = absl::GetFlag(FLAGS_pg_host),
        .port = _port_str,
        .user = absl::GetFlag(FLAGS_pg_user),
        .password = absl::GetFlag(FLAGS_pg_password),
        .dbname = absl::GetFlag(FLAGS_pg_database)});
    _clear_database();
  }

  void _clear_database() {
    PGconn* conn = PQsetdbLogin(
        absl::GetFlag(FLAGS_pg_host).c_str(),
        _port_str.c_str(),
        /*options=*/nullptr,
        /*tty=*/nullptr,
        absl::GetFlag(FLAGS_pg_database).c_str(),
        absl::GetFlag(FLAGS_pg_user).c_str(),
        absl::GetFlag(FLAGS_pg_password).c_str());
    ASSERT_EQ(PQstatus(conn), CONNECTION_OK) << PQerrorMessage(conn);
    PGresult* res = PQexec(
        conn, "TRUNCATE candles, market, trades RESTART IDENTITY CASCADE");
    ASSERT_EQ(PQresultStatus(res), PGRES_COMMAND_OK) << PQerrorMessage(conn);
    PQclear(res);
    PQfinish(conn);
  }

  std::string _port_str;
  std::unique_ptr<postgres_database> _db;
};

TEST_F(PostgresDatabaseTest, InitializesEmptyDatabase) {
  // The constructor of postgres_database should handle the schema
  // initialization. If the constructor doesn't throw, we assume the
  // initialization is successful.
  EXPECT_NE(_db, nullptr);
}

// MARK: Candle

TEST_F(PostgresDatabaseTest, CanSaveCandles) {
  EXPECT_NO_THROW(_db->save(stock::NVDA, Candle::default_instance()).get());
}

TEST_F(PostgresDatabaseTest, SavedCandlesAreReadable) {
  Candle candle;
  candle.set_open(1.0);
  candle.set_close(2.0);
  candle.set_high(3.0);
  candle.set_low(4.0);
  candle.set_volume(5);
  candle.mutable_opened_at()->set_seconds(6);
  candle.mutable_duration()->set_seconds(7);

  _db->save(stock::NVDA, candle).get();

  int count = 0;
  for (const Candle& found_candle : _db->read_candles(stock::NVDA)) {
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

TEST_F(PostgresDatabaseTest, ReadingIteratesOverAllCandlesInOrder) {
  constexpr int CANDLE_COUNT = 100;
  absl::BitGen rand_gen;
  for (int i = 0; i < CANDLE_COUNT; ++i) {
    Candle candle;
    candle.set_open(i);
    candle.mutable_opened_at()->set_seconds(
        absl::Uniform(rand_gen, 0ll, 1ll << 30));
    _db->save(stock::NVDA, candle).get();
  }

  int counter = 0;
  int64_t prev_opened_at = -1;
  for (const Candle& candle : _db->read_candles(stock::NVDA)) {
    ++counter;
    EXPECT_GT(candle.opened_at().seconds(), prev_opened_at);
    prev_opened_at = candle.opened_at().seconds();
  }
  EXPECT_EQ(counter, CANDLE_COUNT);
}

// MARK: Market

TEST_F(PostgresDatabaseTest, CanSaveMarket) {
  EXPECT_NO_THROW(_db->save(Market::default_instance()).get());
}

TEST_F(PostgresDatabaseTest, SavedMarketHistoryIsReadable) {
  Market market;
  market.set_symbol(stock::NVDA);
  market.set_bid(1.0);
  market.set_bid_lots(2);
  market.set_ask(3.0);
  market.set_ask_lots(4);
  market.set_last(5.0);
  market.set_last_lots(6);
  market.mutable_emitted_at()->set_seconds(7);

  _db->save(market).get();

  int count = 0;
  for (const Market& found_market : _db->read_market(stock::NVDA)) {
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

TEST_F(PostgresDatabaseTest, ReadingIteratesOverAllMarketHistoryInOrder) {
  constexpr int MARKET_COUNT = 100;
  absl::BitGen rand_gen;
  for (int i = 0; i < MARKET_COUNT; ++i) {
    Market market;
    market.set_symbol(stock::NVDA);
    market.set_bid(i);
    market.mutable_emitted_at()->set_seconds(
        absl::Uniform(rand_gen, 0ll, 1ll << 30));
    _db->save(market).get();
  }

  int counter = 0;
  int64_t prev_emitted_at = -1;
  for (const Market& market : _db->read_market(stock::NVDA)) {
    ++counter;
    EXPECT_GT(market.emitted_at().seconds(), prev_emitted_at);
    prev_emitted_at = market.emitted_at().seconds();
  }
  EXPECT_EQ(counter, MARKET_COUNT);
}

// MARK: Trade

TEST_F(PostgresDatabaseTest, CanSaveTrade) {
  trading::TradeRecord trade;
  trade.set_symbol(stock::NVDA);
  trade.set_action(trading::BUY);
  trade.set_price(100.0);
  trade.set_quantity(10);
  trade.set_confidence(0.9);
  trade.set_dry_run(true);
  *trade.mutable_executed_at() = to_proto(system_clock::now());

  EXPECT_NO_THROW(_db->save_trade(trade).get());
}

TEST_F(PostgresDatabaseTest, SavedTradesAreReadable) {
  trading::TradeRecord trade;
  trade.set_symbol(stock::NVDA);
  trade.set_action(trading::SELL);
  trade.set_price(200.0);
  trade.set_quantity(20);
  trade.set_confidence(0.95);
  trade.set_dry_run(false);
  *trade.mutable_executed_at() = to_proto(system_clock::now());

  _db->save_trade(trade).get();
  int count = 0;

  for (const trading::TradeRecord& trade : _db->read_trades(stock::NVDA)) {
    ++count;
    EXPECT_EQ(trade.symbol(), stock::NVDA);
    EXPECT_EQ(trade.action(), trading::SELL);
    EXPECT_DOUBLE_EQ(trade.price(), 200.0);
    EXPECT_EQ(trade.quantity(), 20);
    EXPECT_DOUBLE_EQ(trade.confidence(), 0.95);
    EXPECT_FALSE(trade.dry_run());
  }

  EXPECT_EQ(count, 1);
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
