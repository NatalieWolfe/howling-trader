#include "services/db/sqlite_database.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/random/random.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "google/protobuf/util/time_util.h"
#include "services/db/constants.h"
#include "services/db/database_test_interface.h"
#include "services/mock_security.h"
#include "sqlite3.h"
#include "time/conversion.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_DECLARE_FLAG(std::string, sqlite_db_path);

namespace howling {
namespace {

using ::testing::IsSupersetOf;
using ::testing::Return;

class SqliteDatabaseTest : public DatabaseTest {
protected:
  void SetUp() override {
    auto security = std::make_unique<mock_security_client>();
    _mock_security = security.get();
    absl::SetFlag(&FLAGS_sqlite_db_path, ":memory:");
    _db = std::make_unique<sqlite_database>(std::move(security));
    DatabaseTest::SetUp();
  }

  database& db() override { return *_db; }

  void clear_database() override {
    std::string path = absl::GetFlag(FLAGS_sqlite_db_path);
    sqlite3* db;
    if (sqlite3_open(path.c_str(), &db) == SQLITE_OK) {
      sqlite3_exec(db, "DELETE FROM candles", nullptr, nullptr, nullptr);
      sqlite3_exec(db, "DELETE FROM market", nullptr, nullptr, nullptr);
      sqlite3_exec(db, "DELETE FROM trades", nullptr, nullptr, nullptr);
      sqlite3_exec(db, "DELETE FROM auth_tokens", nullptr, nullptr, nullptr);
    }
    sqlite3_close_v2(db);
  }

  std::unique_ptr<sqlite_database> _db;
};

TEST_F(SqliteDatabaseTest, InitializesEmptyDatabase) {
  constexpr std::string_view SHARED_MEMORY_DB_PATH =
      "file::memory:?cache=shared";
  absl::SetFlag(&FLAGS_sqlite_db_path, std::string{SHARED_MEMORY_DB_PATH});

  std::unique_ptr<sqlite_database> db;
  EXPECT_NO_THROW(
      db = std::make_unique<sqlite_database>(
          std::make_unique<mock_security_client>()));
  EXPECT_NO_THROW(db->upgrade_schema("").get());

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

TEST_F(SqliteDatabaseTest, SavedRefreshTokenIsEncryptedAtRest) {
  constexpr std::string_view SHARED_MEMORY_DB_PATH =
      "file:encrypted_at_rest?mode=memory&cache=shared";
  absl::SetFlag(&FLAGS_sqlite_db_path, std::string{SHARED_MEMORY_DB_PATH});

  auto security = std::make_unique<mock_security_client>();
  mock_security_client* security_client = security.get();
  _db = std::make_unique<sqlite_database>(std::move(security));
  _db->upgrade_schema("").get();

  std::string secret_token = "my_very_secret_refresh_token";
  std::string encrypted_token = "vault:v1:encrypted_token";

  EXPECT_CALL(*security_client, encrypt(HOWLING_DB_KEY, secret_token))
      .WillOnce(Return(encrypted_token));

  _db->save_refresh_token("schwab", secret_token).get();

  sqlite3* raw_db;
  ASSERT_EQ(sqlite3_open(SHARED_MEMORY_DB_PATH.data(), &raw_db), SQLITE_OK);

  sqlite3_stmt* stmt;
  ASSERT_EQ(
      sqlite3_prepare_v2(
          raw_db,
          "SELECT refresh_token FROM auth_tokens WHERE service_name = 'schwab'",
          -1,
          &stmt,
          nullptr),
      SQLITE_OK);

  ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
  std::string stored_token{
      reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
      static_cast<size_t>(sqlite3_column_bytes(stmt, 0))};

  EXPECT_EQ(stored_token, encrypted_token);

  sqlite3_finalize(stmt);
  sqlite3_close_v2(raw_db);
}

DATABASE_TEST(SqliteDatabaseTest);

} // namespace
} // namespace howling
