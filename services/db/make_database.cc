#include "services/db/make_database.h"

#include <memory>

#include "services/database.h"
#include "services/db/sqlite_database.h"

namespace howling {

std::unique_ptr<database> make_database() {
  return std::make_unique<sqlite_database>();
}

} // namespace howling
