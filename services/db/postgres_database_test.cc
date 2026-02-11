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
#include "services/db/database_test_interface.h"
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

ABSL_DECLARE_FLAG(std::string, db_encryption_key);

namespace howling {
namespace {

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using ::std::chrono::system_clock;
using ::testing::AllOf;
using ::testing::Property;

class PostgresDatabaseTest : public DatabaseTest {
protected:
  void SetUp() override {
    absl::SetFlag(
        &FLAGS_db_encryption_key,
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
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
        conn,
        "TRUNCATE auth_tokens, candles, market, trades RESTART IDENTITY "
        "CASCADE");
    ASSERT_EQ(PQresultStatus(res), PGRES_COMMAND_OK) << PQerrorMessage(conn);
    PQclear(res);
    PQfinish(conn);
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

  std::string _port_str;
  std::unique_ptr<postgres_database> _db;
};

TEST_F(PostgresDatabaseTest, InitializesEmptyDatabase) {
  // The constructor of postgres_database should handle the schema
  // initialization. If the constructor doesn't throw, we assume the
  // initialization is successful.
  EXPECT_NE(_db, nullptr);
}

TEST_F(PostgresDatabaseTest, SavedRefreshTokenIsEncryptedAtRest) {
  std::string secret_token = "my_very_secret_refresh_token";
  save_refresh_token("schwab", secret_token);

  // Open a raw connection to verify the data in the database.
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
      conn,
      "SELECT refresh_token FROM auth_tokens WHERE service_name = 'schwab'");
  ASSERT_EQ(PQresultStatus(res), PGRES_TUPLES_OK) << PQerrorMessage(conn);
  ASSERT_EQ(PQntuples(res), 1);

  std::string stored_token{
      PQgetvalue(res, 0, 0), static_cast<size_t>(PQgetlength(res, 0, 0))};

  // The stored token should NOT be the secret token.
  EXPECT_NE(stored_token, secret_token);

  // Ensure it can still be read back through the API correctly.
  EXPECT_EQ(read_refresh_token("schwab"), secret_token);

  PQclear(res);
  PQfinish(conn);
}

DATABASE_TEST(PostgresDatabaseTest);

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
