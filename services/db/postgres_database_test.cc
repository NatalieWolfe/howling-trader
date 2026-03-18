#include "services/db/postgres_database.h"

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/random/random.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "google/protobuf/util/time_util.h"
#include "libpq/libpq-fe.h"
#include "services/db/constants.h"
#include "services/db/database_test_interface.h"
#include "services/mock_security.h"
#include "time/conversion.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

ABSL_FLAG(std::string, pg_host, "localhost", "Hostname for Postgres.");
ABSL_FLAG(std::string, pg_port, "5432", "Port for Postgres.");
ABSL_FLAG(std::string, pg_user, "postgres", "User for Postgres.");
ABSL_FLAG(std::string, pg_password, "password", "Password for Postgres.");
ABSL_FLAG(std::string, pg_database, "howling", "Database for Postgres.");

namespace howling {
namespace {

using ::testing::IsSupersetOf;
using ::testing::Return;

class PostgresDatabaseTest : public DatabaseTest {
protected:
  void SetUp() override {
    postgres_options options{
        .host = absl::GetFlag(FLAGS_pg_host),
        .port = absl::GetFlag(FLAGS_pg_port),
        .user = absl::GetFlag(FLAGS_pg_user),
        .password = absl::GetFlag(FLAGS_pg_password),
        .dbname = absl::GetFlag(FLAGS_pg_database),
        .sslmode = "disable"};
    auto security = std::make_unique<mock_security_client>();
    _mock_security = security.get();
    _db = std::make_unique<postgres_database>(options, std::move(security));
    DatabaseTest::SetUp();
  }

  database& db() override { return *_db; }

  void clear_database() override {
    std::string conninfo = std::format(
        "host={} port={} dbname={} user={} password={} sslmode=disable",
        absl::GetFlag(FLAGS_pg_host),
        absl::GetFlag(FLAGS_pg_port),
        absl::GetFlag(FLAGS_pg_database),
        absl::GetFlag(FLAGS_pg_user),
        absl::GetFlag(FLAGS_pg_password));
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) == CONNECTION_OK) {
      PGresult* res =
          PQexec(conn, "TRUNCATE candles, market, trades, auth_tokens");
      PQclear(res);
    }
    PQfinish(conn);
  }

  std::unique_ptr<postgres_database> _db;
};

TEST_F(PostgresDatabaseTest, InitializesEmptyDatabase) {
  db().upgrade_schema("").get();
  std::string conninfo = std::format(
      "host={} port={} dbname={} user={} password={} sslmode=disable",
      absl::GetFlag(FLAGS_pg_host),
      absl::GetFlag(FLAGS_pg_port),
      absl::GetFlag(FLAGS_pg_database),
      absl::GetFlag(FLAGS_pg_user),
      absl::GetFlag(FLAGS_pg_password));
  PGconn* conn = PQconnectdb(conninfo.c_str());
  ASSERT_EQ(PQstatus(conn), CONNECTION_OK) << PQerrorMessage(conn);

  std::unordered_set<std::string> found_tables;
  PGresult* res = PQexec(
      conn,
      "SELECT table_name FROM information_schema.tables WHERE table_schema = "
      "'public'");
  ASSERT_EQ(PQresultStatus(res), PGRES_TUPLES_OK) << PQerrorMessage(conn);

  for (int i = 0; i < PQntuples(res); ++i) {
    found_tables.insert(PQgetvalue(res, i, 0));
  }
  EXPECT_THAT(
      found_tables,
      IsSupersetOf({"howling_version", "auth_tokens", "candles", "trades"}));

  PQclear(res);
  PQfinish(conn);
}

TEST_F(PostgresDatabaseTest, SavedRefreshTokenIsEncryptedAtRest) {
  db().upgrade_schema("").get();
  std::string secret_token = "my_very_secret_refresh_token";
  std::string encrypted_token = "vault:v1:encrypted_token";

  EXPECT_CALL(*_mock_security, encrypt(HOWLING_DB_KEY, secret_token))
      .WillOnce(Return(encrypted_token));

  _db->save_refresh_token("schwab", secret_token).get();

  std::string conninfo = std::format(
      "host={} port={} dbname={} user={} password={} sslmode=disable",
      absl::GetFlag(FLAGS_pg_host),
      absl::GetFlag(FLAGS_pg_port),
      absl::GetFlag(FLAGS_pg_database),
      absl::GetFlag(FLAGS_pg_user),
      absl::GetFlag(FLAGS_pg_password));
  PGconn* conn = PQconnectdb(conninfo.c_str());
  ASSERT_EQ(PQstatus(conn), CONNECTION_OK) << PQerrorMessage(conn);

  PGresult* res = PQexec(
      conn,
      "SELECT refresh_token FROM auth_tokens WHERE service_name = 'schwab'");
  ASSERT_EQ(PQresultStatus(res), PGRES_TUPLES_OK) << PQerrorMessage(conn);
  ASSERT_EQ(PQntuples(res), 1);

  size_t unescaped_len;
  unsigned char* unescaped = PQunescapeBytea(
      reinterpret_cast<const unsigned char*>(PQgetvalue(res, 0, 0)),
      &unescaped_len);

  std::string stored_token{
      reinterpret_cast<const char*>(unescaped), unescaped_len};
  PQfreemem(unescaped);

  EXPECT_EQ(stored_token, encrypted_token);

  PQclear(res);
  PQfinish(conn);
}

DATABASE_TEST(PostgresDatabaseTest);

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);
  return RUN_ALL_TESTS();
}
