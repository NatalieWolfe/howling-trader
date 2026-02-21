#include "services/db/schema/schema.h"

#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>

#include <iostream>

#include "absl/strings/str_join.h"
#include "sqlite3.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling::db_internal {
namespace {

using ::testing::StartsWith;

TEST(GetSchemaVersion, MatchesFullSchemaInsert) {
  sqlite3* db;
  ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);

  for (std::string_view command : get_full_schema()) {
    char* error_message = nullptr;
    int res = sqlite3_exec(
        db, std::string{command}.c_str(), nullptr, nullptr, &error_message);
    if (res != SQLITE_OK) {
      std::string error = error_message;
      sqlite3_free(error_message);
      sqlite3_close(db);
      FAIL() << "Failed to execute schema command: " << command << " (" << error
             << ")";
    }
  }

  sqlite3_stmt* stmt;
  ASSERT_EQ(
      sqlite3_prepare_v2(
          db, "SELECT v FROM howling_version", -1, &stmt, nullptr),
      SQLITE_OK);
  ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
  int db_version = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  EXPECT_EQ(get_schema_version(), db_version);
}

TEST(GetFullSchema, StartsWithCreateVersionTable) {
  EXPECT_THAT(
      *get_full_schema().begin(), StartsWith("CREATE TABLE howling_version "));
}

TEST(GetFullSchema, InsertsVersion) {
  std::string insert_line;
  for (std::string_view command : get_full_schema()) {
    if (command.contains("INSERT INTO howling_version ")) {
      EXPECT_TRUE(insert_line.empty()) << "Multiple version insertions found!";
      insert_line = std::string{command};
    }
  }
  EXPECT_FALSE(insert_line.empty()) << "No version insertion found!";
}

TEST(GetFullSchema, CreatesAllExpectedTables) {
  const std::unordered_set<std::string> expected_tables{
      "howling_version", "auth_tokens", "candles", "market", "trades"};
  std::unordered_set<std::string> missing_tables = expected_tables;
  std::regex table_regex{R"re(CREATE TABLE (\w+))re"};
  for (std::string_view command : get_full_schema()) {
    std::cmatch match;
    if (std::regex_search(command.begin(), command.end(), match, table_regex)) {
      std::string table_name = match.str(1);
      EXPECT_TRUE(expected_tables.contains(table_name))
          << "Unexpected table definition for " << table_name;
      EXPECT_TRUE(missing_tables.contains(table_name))
          << "Duplicate creation for table " << table_name;
      missing_tables.erase(table_name);
    }
  }
  EXPECT_TRUE(missing_tables.empty()) << "Missing definitions for table(s): "
                                      << absl::StrJoin(missing_tables, ", ");
}

TEST(GetSchemaUpdate, ReturnsUpdatesForVersion1) {
  bool found_auth_tokens = false;
  for (std::string_view command : get_schema_update(1)) {
    if (command.contains("CREATE TABLE auth_tokens")) {
      found_auth_tokens = true;
    }
  }
  EXPECT_TRUE(found_auth_tokens);
}

} // namespace
} // namespace howling::db_internal
