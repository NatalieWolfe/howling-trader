#pragma once

namespace howling {

struct use_admin_database_account_t {
  explicit use_admin_database_account_t() = default;
};

/**
 * @brief Indicates that the admin database account should be used.
 */
inline constexpr use_admin_database_account_t use_admin_database_account{};

/**
 * @brief Registers the database client in the service registry.
 */
void register_database_client();

/**
 * @brief Registers the database client in the service registry using the
 * administrative credentials to connect.
 */
void register_database_client(use_admin_database_account_t);

} // namespace howling
