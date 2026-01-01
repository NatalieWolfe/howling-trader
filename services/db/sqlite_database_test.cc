#include "services/db/sqlite_database.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/random/random.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "sqlite3.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_DECLARE_FLAG(std::string, sqlite_db_path);

namespace howling {
namespace {

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
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
  EXPECT_THAT(found_tables, IsSupersetOf({"howling_version", "candles"}));
  sqlite3_close_v2(raw_db);
}

TEST(SqliteDatabase, CanSaveCandles) {
  set_memory_database_path();
  sqlite_database db;

  EXPECT_NO_THROW(db.save(stock::NVDA, Candle::default_instance()).get());
}

TEST(SqliteDatabase, SavedCandlesAreReadable) {
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

TEST(SqliteDatabase, ReadingIteratesOverAllCandlesInOrder) {
  set_memory_database_path();
  sqlite_database db;

  constexpr int CANDLE_COUNT = 100;
  absl::BitGen rand_gen;
  for (int i = 0; i < CANDLE_COUNT; ++i) {
    Candle candle;
    candle.set_open(i);
    candle.mutable_opened_at()->set_seconds(
        absl::Uniform(rand_gen, 0ll, 1ll << 62));
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

} // namespace
} // namespace howling
