#include <exception>
#include <iostream>
#include <memory>

#include "absl/log/log.h"
#include "environment/init.h"
#include "services/db/make_database.h"
#include "services/security/bao_client.h"

namespace howling {
namespace {

void run() {
  using namespace std::chrono_literals;

  LOG(INFO) << "Waiting for security client to be ready...";
  auto security = std::make_unique<security::bao_client>();
  security->wait_for_ready(30s);
  LOG(INFO) << "Starting schema upgrade...";
  auto db = make_database(use_admin_database_account, std::move(security));
  LOG(INFO) << "Schema upgrade completed successfully.";
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
