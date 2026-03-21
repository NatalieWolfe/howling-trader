#include <chrono>
#include <exception>
#include <iostream>
#include <memory>

#include "absl/log/log.h"
#include "api/schwab/configuration.h"
#include "environment/init.h"
#include "services/authenticate.h"
#include "services/database.h"
#include "services/db/make_database.h"
#include "services/oauth/auth_client.h"
#include "services/security/register.h"

namespace howling {
namespace {

void run() {
  security::register_security_client();
  schwab::fetch_schwab_secrets();

  LOG(INFO) << "Initializing database connection.";
  std::unique_ptr<database> db = make_database();
  db->check_schema_version().get();

  LOG(INFO) << "Refreshing token.";
  token_manager token_man{
      make_auth_service_stub(), std::move(db), /*refresher=*/nullptr};
  token_man.get_bearer_token();
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);

  try {
    howling::run();
    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Auth refresh failed: " << e.what();
    std::cerr << "Auth refresh failed: " << e.what() << std::endl;
  } catch (...) {
    LOG(ERROR) << "Auth refresh failed with unknown error.";
    std::cerr << "Auth refresh failed with unknown error." << std::endl;
  }
  return 1;
}
