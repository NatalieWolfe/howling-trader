#pragma once

#include <memory>

#include "services/database.h"
#include "services/security.h"

namespace howling {

std::unique_ptr<database>
make_database(std::unique_ptr<security_client> security);

}
