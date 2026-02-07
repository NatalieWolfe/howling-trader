#include "services/db/make_database.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "services/database.h"
#include "services/db/postgres_database.h"
#include "services/db/sqlite_database.h"

ABSL_FLAG(std::string, database, "sqlite", "Type of database to use (sqlite, postgres)");

ABSL_FLAG(std::string, pg_user, "postgres", "User to connect as to the Postgres database.");
ABSL_FLAG(std::string, pg_password, "password", "User password to connect to the Postgres database.");
ABSL_FLAG(std::string, pg_host, "localhost", "Hostname for the Postgres database.");
ABSL_FLAG(int, pg_port, 5432, "Port to connect to for the Postgres database.");
ABSL_FLAG(std::string, pg_database, "howling", "Name of the Postgres database.");

namespace howling {

std::unique_ptr<database> make_database() {
  std::string type = absl::GetFlag(FLAGS_database);
  if (type == "postgres") {
    return std::make_unique<postgres_database>(postgres_options{
        .host = absl::GetFlag(FLAGS_pg_host),
        .port = std::to_string(absl::GetFlag(FLAGS_pg_port)),
        .user = absl::GetFlag(FLAGS_pg_user),
        .password = absl::GetFlag(FLAGS_pg_password),
        .dbname = absl::GetFlag(FLAGS_pg_database)});
  }
  return std::make_unique<sqlite_database>();
}

} // namespace howling
