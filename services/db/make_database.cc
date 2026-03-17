#include "services/db/make_database.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "services/database.h"
#include "services/db/postgres_database.h"
#include "services/db/sqlite_database.h"
#include "services/security.h"

ABSL_FLAG(
    std::string,
    database,
    "sqlite",
    "Type of database to use (sqlite, postgres)");

ABSL_FLAG(
    std::string,
    pg_user,
    "postgres",
    "User to connect as to the Postgres database.");
ABSL_FLAG(
    std::string,
    pg_password,
    "password",
    "User password to connect to the Postgres database.");
ABSL_FLAG(
    std::string, pg_host, "localhost", "Hostname for the Postgres database.");
ABSL_FLAG(int, pg_port, 5432, "Port to connect to for the Postgres database.");
ABSL_FLAG(
    std::string, pg_database, "howling", "Name of the Postgres database.");
ABSL_FLAG(
    bool,
    pg_enable_encryption,
    false,
    "Enable encryption for the Postgres connection.");

namespace howling {
namespace {

void fetch_db_secrets_internal(
    security_client& security, std::string_view path) {
  if (absl::GetFlag(FLAGS_pg_user) != "postgres" ||
      absl::GetFlag(FLAGS_pg_password) != "password") {
    LOG(INFO) << "Database secrets already provided via flags, skipping "
                 "OpenBao fetch.";
    return;
  }

  LOG(INFO) << "Fetching database secrets from OpenBao path: " << path;
  Json::Value secret = security.get_secret(path);
  if (secret.isMember("username") && secret["username"].isString()) {
    absl::SetFlag(&FLAGS_pg_user, secret["username"].asString());
  }
  if (secret.isMember("password") && secret["password"].isString()) {
    absl::SetFlag(&FLAGS_pg_password, secret["password"].asString());
  }
}

void fetch_database_secrets(security_client& security) {
  fetch_db_secrets_internal(security, "howling/prod/database");
}

void fetch_admin_database_secrets(security_client& security) {
  fetch_db_secrets_internal(security, "howling/admin/database");
}

std::unique_ptr<database>
make_database_internal(std::unique_ptr<security_client> security) {
  if (absl::GetFlag(FLAGS_database) == "postgres") {
    bool encrypt = absl::GetFlag(FLAGS_pg_enable_encryption);
    LOG(INFO) << "Connecting to postgres database \""
              << absl::GetFlag(FLAGS_pg_database) << "\" at "
              << absl::GetFlag(FLAGS_pg_host) << ":"
              << absl::GetFlag(FLAGS_pg_port)
              << (encrypt ? " (encrypted)" : " (unencrypted)");
    return std::make_unique<postgres_database>(
        postgres_options{
            .host = absl::GetFlag(FLAGS_pg_host),
            .port = std::to_string(absl::GetFlag(FLAGS_pg_port)),
            .user = absl::GetFlag(FLAGS_pg_user),
            .password = absl::GetFlag(FLAGS_pg_password),
            .dbname = absl::GetFlag(FLAGS_pg_database),
            .sslmode = encrypt ? "require" : "disable"},
        std::move(security));
  }
  return std::make_unique<sqlite_database>(std::move(security));
}

} // namespace

std::unique_ptr<database>
make_database(std::unique_ptr<security_client> security) {
  if (security && absl::GetFlag(FLAGS_database) == "postgres") {
    fetch_database_secrets(*security);
  }
  return make_database_internal(std::move(security));
}

std::unique_ptr<database> make_database(
    use_admin_database_account_t, std::unique_ptr<security_client> security) {
  if (security && absl::GetFlag(FLAGS_database) == "postgres") {
    fetch_admin_database_secrets(*security);
  }
  return make_database_internal(std::move(security));
}

} // namespace howling
