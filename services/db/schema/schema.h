#pragma once

#include <generator>
#include <string_view>

namespace howling::db_internal {

int get_schema_version();
std::generator<std::string_view> get_full_schema();

} // namespace howling::db_internal
