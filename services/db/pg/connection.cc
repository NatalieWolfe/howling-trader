#include "services/db/pg/connection.h"

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

#include "absl/log/log.h"

namespace howling::pg {
namespace {

std::string escape_conninfo_value(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('\'');
  for (char character : value) {
    if (character == '\\' || character == '\'') { escaped.push_back('\\'); }
    escaped.push_back(character);
  }
  escaped.push_back('\'');
  return escaped;
}

} // namespace

void conn_deleter::operator()(PGconn* conn) const noexcept {
  if (conn != nullptr) { PQfinish(conn); }
}

std::string build_conninfo(const postgres_options& options) {
  return std::format(
      "host={} port={} dbname={} user={} password={} sslmode={}",
      escape_conninfo_value(options.host),
      escape_conninfo_value(options.port),
      escape_conninfo_value(options.dbname),
      escape_conninfo_value(options.user),
      escape_conninfo_value(options.password),
      escape_conninfo_value(options.sslmode));
}

unique_conn connect(const postgres_options& options) {
  std::string conninfo = build_conninfo(options);
  unique_conn conn{PQconnectdb(conninfo.c_str())};
  if (!conn) {
    throw std::runtime_error("PQconnectdb returned null connection handle.");
  }
  check_connection_error(*conn);
  LOG(INFO) << "Postgres connection established. SSL in use: "
            << (PQsslInUse(conn.get()) ? "yes" : "no");
  return conn;
}

void check_connection_error(PGconn& conn) {
  ConnStatusType status = PQstatus(&conn);
  if (status != CONNECTION_OK) {
    throw std::runtime_error(
        std::format(
            "Postgres connection error ({}) {}",
            static_cast<int>(status),
            PQerrorMessage(&conn)));
  }
}

void check_command_result(PGresult* res, PGconn& conn) {
  ExecStatusType status = PQresultStatus(res);
  if (status != PGRES_COMMAND_OK) {
    std::string message = PQerrorMessage(&conn);
    PQclear(res);
    throw std::runtime_error(
        std::format(
            "Postgres result error ({} / {}) {}",
            PQresStatus(status),
            static_cast<int>(status),
            message));
  }
  PQclear(res);
}

void check_not_null(PGresult* res, int row, int col) {
  if (PQgetisnull(res, row, col)) {
    throw std::runtime_error(
        std::format(
            "Unexpected NULL value in DB at row {}, col {}.", row, col));
  }
}

void execute(PGconn& conn, std::string_view query_str) {
  std::string sql{query_str};
  check_command_result(PQexec(&conn, sql.c_str()), conn);
}

std::string escape_identifier(PGconn& conn, std::string_view identifier) {
  char* escaped =
      PQescapeIdentifier(&conn, identifier.data(), identifier.length());
  if (escaped == nullptr) {
    throw std::runtime_error("Failed to escape identifier.");
  }
  std::string result{escaped};
  PQfreemem(escaped);
  return result;
}

void rollback_if_in_transaction(PGconn& conn) {
  check_connection_error(conn);
  if (PQtransactionStatus(&conn) != PQTRANS_IDLE) {
    check_command_result(PQexec(&conn, "ROLLBACK"), conn);
  }
  PGTransactionStatusType status = PQtransactionStatus(&conn);
  if (status != PQTRANS_IDLE) {
    throw std::runtime_error(
        std::format(
            "Postgres connection remained in non-idle transaction state ({}) "
            "after ROLLBACK.",
            static_cast<int>(status)));
  }
}

} // namespace howling::pg
