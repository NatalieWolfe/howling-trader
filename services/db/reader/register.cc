#include "services/db/reader/register.h"

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "services/db/environment.h"
#include "services/db/pg/connection.h"
#include "services/db/reader/postgres_reader_db.h"
#include "services/db/reader/reader_db.h"
#include "services/registry/register_service.h"

namespace howling {
namespace {

std::unique_ptr<reader_db> create_reader_db() {
  bool encrypt = absl::GetFlag(FLAGS_pg_enable_encryption);
  int port = absl::GetFlag(FLAGS_pg_port);
  LOG(INFO) << "Connecting read-only to postgres database \""
            << absl::GetFlag(FLAGS_pg_database) << "\" at "
            << absl::GetFlag(FLAGS_pg_host) << ":" << port
            << (encrypt ? " (encrypted)" : " (unencrypted)");
  return std::make_unique<postgres_reader_db>(postgres_options{
      .host = absl::GetFlag(FLAGS_pg_host),
      .port = std::to_string(port),
      .user = absl::GetFlag(FLAGS_pg_user),
      .password = absl::GetFlag(FLAGS_pg_password),
      .dbname = absl::GetFlag(FLAGS_pg_database),
      .sslmode = encrypt ? "require" : "disable"});
}

} // namespace

void register_reader_db() {
  registry::register_service_factory(
      []() -> std::unique_ptr<reader_db> { return create_reader_db(); });

  registry::register_service_factory([]() -> std::unique_ptr<reader_db_pool> {
    LOG(INFO) << "Initializing read-only database connection pool.";
    return std::make_unique<reader_db_pool>(
        []() -> std::unique_ptr<reader_db> { return create_reader_db(); },
        reader_db_pool::options{
            .min_size = absl::GetFlag(FLAGS_db_pool_min_size),
            .max_size = absl::GetFlag(FLAGS_db_pool_max_size),
        });
  });
}

} // namespace howling
