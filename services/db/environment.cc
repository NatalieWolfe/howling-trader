#include "services/db/environment.h"

#include <cstddef>
#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(
    std::string,
    db_encryption_key_name,
    "default",
    "The name of the encryption key to be used by the database.");
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
ABSL_FLAG(
    size_t,
    db_pool_min_size,
    1,
    "Minimum number of database connections in pool.");
ABSL_FLAG(
    size_t,
    db_pool_max_size,
    10,
    "Maximum number of database connections in pool.");
