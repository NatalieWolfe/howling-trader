#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "libpq/libpq-fe.h"

namespace howling {

/**
 * @brief Connection parameters for a PostgreSQL server.
 */
struct postgres_options {
  std::string host;
  std::string port;
  std::string user;
  std::string password;
  std::string dbname;
  std::string sslmode;
};

namespace pg {

/**
 * @brief Custom deleter for PGconn pointers.
 */
struct conn_deleter {
  void operator()(PGconn* conn) const noexcept;
};

/**
 * @brief Owning smart pointer for a PostgreSQL connection.
 */
using unique_conn = std::unique_ptr<PGconn, conn_deleter>;

/**
 * @brief Builds a libpq key=value connection string from options.
 *
 * Each value is single-quoted with backslashes and single quotes escaped
 * according to libpq conninfo quoting rules.
 */
[[nodiscard]] std::string build_conninfo(const postgres_options& options);

/**
 * @brief Connects to PostgreSQL and verifies the connection status.
 *
 * @throws std::runtime_error if the connection cannot be established.
 */
[[nodiscard]] unique_conn connect(const postgres_options& options);

/**
 * @brief Verifies that a PGconn is in CONNECTION_OK state.
 *
 * @throws std::runtime_error if the connection is in an error state.
 */
void check_connection_error(PGconn& conn);

/**
 * @brief Verifies that a PGresult succeeded with PGRES_COMMAND_OK and clears
 * it.
 *
 * Always calls PQclear(res) before returning or throwing.
 *
 * @throws std::runtime_error if the command did not return PGRES_COMMAND_OK.
 */
void check_command_result(PGresult* res, PGconn& conn);

/**
 * @brief Throws std::runtime_error if the specified result cell is SQL NULL.
 */
void check_not_null(PGresult* res, int row, int col);

/**
 * @brief Executes a parameterless SQL command and checks for PGRES_COMMAND_OK.
 */
void execute(PGconn& conn, std::string_view query_str);

/**
 * @brief Safely escapes a SQL identifier using PQescapeIdentifier.
 */
[[nodiscard]] std::string
escape_identifier(PGconn& conn, std::string_view identifier);

/**
 * @brief Rolls back any open or aborted transaction on the connection.
 *
 * Verifies that the connection is healthy, executes ROLLBACK if
 * PQtransactionStatus is not PQTRANS_IDLE, and confirms the connection returns
 * to PQTRANS_IDLE so broken or poisoned connections are discarded by pools.
 *
 * @throws std::runtime_error if the connection is unhealthy, ROLLBACK fails, or
 *   the connection remains in a non-idle transaction state.
 */
void rollback_if_in_transaction(PGconn& conn);

} // namespace pg
} // namespace howling
