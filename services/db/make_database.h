#pragma once

#include <memory>

#include "services/database.h"

namespace howling {

std::unique_ptr<database> make_database();

}
