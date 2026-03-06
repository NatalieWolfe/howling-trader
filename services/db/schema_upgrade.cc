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
  LOG(INFO) << "Starting schema upgrade...";
  auto db = make_database(std::make_unique<security::bao_client>());
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
