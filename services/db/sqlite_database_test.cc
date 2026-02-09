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
#include "services/db/database_test_interface.h"
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

class SqliteDatabaseTest : public DatabaseTest {
protected:
  void SetUp() override {
    set_memory_database_path();
    _db = std::make_unique<sqlite_database>();
  }

  void save_candle(stock::Symbol symbol, const Candle& candle) override {
    _db->save(symbol, candle).get();
  }
  void save_market(const Market& market) override { _db->save(market).get(); }
  void save_trade(const trading::TradeRecord& trade) override {
    _db->save_trade(trade).get();
  }
  void save_refresh_token(
      std::string_view service_name, std::string_view token) override {
    _db->save_refresh_token(service_name, token).get();
  }

  std::generator<Candle> read_candles(stock::Symbol symbol) override {
    return _db->read_candles(symbol);
  }
  std::generator<Market> read_market(stock::Symbol symbol) override {
    return _db->read_market(symbol);
  }
  std::generator<trading::TradeRecord>
  read_trades(stock::Symbol symbol) override {
    return _db->read_trades(symbol);
  }
  std::string read_refresh_token(std::string_view service_name) override {
    return _db->read_refresh_token(service_name).get();
  }

  std::unique_ptr<sqlite_database> _db;
};

TEST_F(SqliteDatabaseTest, InitializesEmptyDatabase) {
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
      found_tables,
      IsSupersetOf({"howling_version", "auth_tokens", "candles", "trades"}));
  sqlite3_close_v2(raw_db);
}

DATABASE_TEST(SqliteDatabaseTest);

} // namespace
} // namespace howling
