#pragma once

#include <string_view>

namespace howling {

/**
 * @brief The key name used in the security client for database encryption.
 */
constexpr std::string_view HOWLING_DB_KEY = "howling-db-key";

} // namespace howling
