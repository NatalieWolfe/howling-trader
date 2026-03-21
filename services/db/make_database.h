#pragma once

#include <memory>

#include "services/database.h"

namespace howling {

/**
 * @brief Tag type for indicating that the admin database account should be
 * used.
 */
struct use_admin_database_account_t {
  explicit use_admin_database_account_t() = default;
};

/**
 * @brief Constant for indicating that the admin database account should be
 * used.
 */
inline constexpr use_admin_database_account_t use_admin_database_account{};

/**
 * @brief Creates a database instance, fetching credentials from OpenBao if
 * a security client is provided.
 */
std::unique_ptr<database> make_database();

/**
 * @brief Creates a database instance using administrative credentials,
 * fetching them from OpenBao if a security client is provided.
 */
std::unique_ptr<database> make_database(use_admin_database_account_t);

} // namespace howling
