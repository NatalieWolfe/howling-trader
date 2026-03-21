#include <chrono>
#include <exception>
#include <iostream>
#include <memory>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "environment/init.h"
#include "services/db/make_database.h"
#include "services/security/register.h"

ABSL_FLAG(
    std::string,
    app_db_user,
    "",
    "Username which non-admin applications will use to connect to the "
    "database.");

namespace howling {
namespace {

void run() {
  using namespace std::chrono_literals;
  security::register_security_client();

  auto db = make_database(use_admin_database_account);
  LOG(INFO) << "Starting schema upgrade...";
  std::future<void> upgrade_state =
      db->upgrade_schema(absl::GetFlag(FLAGS_app_db_user));
  if (upgrade_state.wait_for(10min) == std::future_status::ready) {
    try {
      upgrade_state.get();
      LOG(INFO) << "Schema upgrade completed successfully.";
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to upgrade schema: " << e.what();
      LOG(ERROR) << "Sleeping to enable retain logs for viewing.";
      std::this_thread::sleep_for(10min);
      throw;
    } catch (...) {
      LOG(ERROR) << "Unknown failure while upgrading schema!";
      LOG(ERROR) << "Sleeping to enable retain logs for viewing.";
      std::this_thread::sleep_for(10min);
      throw;
    }
  }
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);

  try {
    howling::run();
    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Upgrade failed: " << e.what();
    std::cerr << "Upgrade failed: " << e.what() << std::endl;
  } catch (...) {
    LOG(ERROR) << "Upgrade failed with unknown error.";
    std::cerr << "Upgrade failed with unknown error." << std::endl;
  }
  return 1;
}
