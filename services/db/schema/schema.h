#pragma once

#include <generator>
#include <string>

namespace howling::db_internal {

int get_schema_version();
std::generator<std::string> get_full_schema();
std::generator<std::string> get_schema_update(int from_version);

} // namespace howling::db_internal
