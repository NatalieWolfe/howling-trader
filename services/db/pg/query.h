#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "libpq/libpq-fe.h"
#include "services/db/pg/connection.h"
#include "services/db/pg/types.h"

namespace howling::pg {

/**
 * @brief Accumulates binary-format parameters for PQexecPrepared and
 * PQexecParams.
 */
class param_binder {
public:
  /**
   * @brief Clears all bound parameters and internal value buffers.
   */
  void clear();

  /**
   * @brief Pre-allocates metadata storage for the expected parameter count.
   */
  void reserve(size_t count);

  /**
   * @brief Binds a 32-bit signed integer parameter (INT4).
   */
  void bind(int32_t value);

  /**
   * @brief Binds a 64-bit signed integer parameter (INT8).
   */
  void bind(int64_t value);

  /**
   * @brief Binds a double-precision floating-point parameter (FLOAT8).
   */
  void bind(double value);

  /**
   * @brief Binds a boolean parameter (BOOL).
   */
  void bind(bool value);

  /**
   * @brief Binds a null-terminated C-string parameter as TEXT.
   *
   * @throws std::invalid_argument if value is nullptr.
   */
  void bind(const char* value);

  /**
   * @brief Disallows binding arbitrary pointer types as booleans.
   */
  void bind(const void*) = delete;

  /**
   * @brief Binds a string view parameter as TEXT.
   */
  void bind(std::string_view value);

  /**
   * @brief Binds a std::string parameter as TEXT.
   */
  void bind(const std::string& value);

  /**
   * @brief Binds a raw byte sequence parameter as BYTEA.
   */
  void bind(const bytes& value);

  /**
   * @brief Binds a UTC time_point parameter as TIMESTAMP.
   */
  void bind(std::chrono::system_clock::time_point value);

  /**
   * @brief Binds an optional parameter, emitting SQL NULL when disengaged.
   */
  template <typename T>
  void bind(const std::optional<T>& value) {
    if (value) {
      bind(*value);
    } else {
      _param_types.push_back(pg_type_id_for<T>());
      _param_values.push_back(nullptr);
      _param_lengths.push_back(0);
      _param_formats.push_back(BINARY_FORMAT);
    }
  }

  /**
   * @brief Returns the number of currently bound parameters.
   */
  [[nodiscard]] int count() const noexcept {
    return static_cast<int>(_param_values.size());
  }

  /**
   * @brief Returns the array of PostgreSQL type OIDs, or nullptr when empty.
   */
  [[nodiscard]] const Oid* types() const noexcept {
    return _param_types.empty() ? nullptr : _param_types.data();
  }

  /**
   * @brief Returns the array of binary value pointers, or nullptr when empty.
   */
  [[nodiscard]] const char* const* values() const noexcept {
    return _param_values.empty() ? nullptr : _param_values.data();
  }

  /**
   * @brief Returns the array of binary byte lengths, or nullptr when empty.
   */
  [[nodiscard]] const int* lengths() const noexcept {
    return _param_lengths.empty() ? nullptr : _param_lengths.data();
  }

  /**
   * @brief Returns the array of parameter format codes, or nullptr when empty.
   */
  [[nodiscard]] const int* formats() const noexcept {
    return _param_formats.empty() ? nullptr : _param_formats.data();
  }

private:
  void _store(unsigned int oid, std::string encoded);

  std::deque<std::string> _buffers;
  std::vector<Oid> _param_types;
  std::vector<const char*> _param_values;
  std::vector<int> _param_lengths;
  std::vector<int> _param_formats;
};

/**
 * @brief Executes prepared or one-shot parameterized binary PostgreSQL queries.
 *
 * Prepared queries use PQprepare / PQexecPrepared for fixed hot-path SQL.
 * One-shot queries use PQexecParams (unnamed parameterized queries) to avoid
 * prepared statement leaks and hash collisions on dynamic SQL.
 *
 * Thread safety: thread-compatible. Distinct instances on separate connections
 * may be used concurrently, but only one active query may execute at a time on
 * a given PGconn and individual query instances are not internally
 * synchronized.
 */
class query {
private:
  struct prepared_tag_t {};
  static constexpr prepared_tag_t PREPARED_TAG{};

public:
  /**
   * @brief Prepares a named statement on the connection for repeated execution.
   *
   * Statements are named by hashing the SQL string and are never deallocated
   * with DEALLOCATE, so each distinct SQL string may only be prepared once per
   * PGconn lifetime.
   */
  template <typename... Args>
  static std::unique_ptr<query>
  prepare(PGconn& conn, std::string_view query_str) {
    std::array<Oid, sizeof...(Args)> type_ids{pg_type_id_for<Args>()...};
    std::string sql{query_str};
    std::string name = _to_prepared_name(sql);
    check_command_result(
        PQprepare(
            &conn,
            name.c_str(),
            sql.c_str(),
            static_cast<int>(type_ids.size()),
            type_ids.empty() ? nullptr : type_ids.data()),
        conn);
    return std::make_unique<query>(PREPARED_TAG, &conn, std::move(name));
  }

  /**
   * @brief Internal constructor for prepared queries.
   */
  query(prepared_tag_t, PGconn* conn, std::string name);

  /**
   * @brief Constructs a one-shot parameterized query executed via PQexecParams.
   */
  query(PGconn& conn, std::string_view query_str);

  ~query();

  query(const query&) = delete;
  query& operator=(const query&) = delete;
  query(query&&) = delete;
  query& operator=(query&&) = delete;

  /**
   * @brief Executes the query and resets the row cursor.
   */
  void execute();

  /**
   * @brief Advances to the next result row, executing the query if needed.
   */
  bool step();

  /**
   * @brief Returns the number of rows in the current result set.
   */
  [[nodiscard]] int row_count() const noexcept { return _total_rows; }

  /**
   * @brief Resets bound parameters and binds all arguments in order.
   */
  template <typename... Args>
  void bind_all(Args&&... args) {
    _clear_result();
    _params.clear();
    if constexpr (sizeof...(Args) > 0) { _params.reserve(sizeof...(Args)); }
    (_params.bind(std::forward<Args>(args)), ...);
  }

  /**
   * @brief Appends a single parameter for dynamically constructed SQL queries.
   */
  template <typename T>
  void add_param(T&& value) {
    _clear_result();
    _params.bind(std::forward<T>(value));
  }

  /**
   * @brief Reads all columns of the current row into the provided outputs.
   *
   * Validates the result column count as well as PostgreSQL column OIDs and
   * byte lengths for every column.
   *
   * @throws std::out_of_range if there is no active result row.
   * @throws std::runtime_error if the column count, OID, or length mismatches.
   */
  template <typename... Outputs>
  void read_all(Outputs&... outputs) {
    if (_res == nullptr || _current_row < 0 || _current_row >= _total_rows) {
      throw std::out_of_range("Current row is out of range.");
    }
    int actual_fields = PQnfields(_res);
    if (static_cast<int>(sizeof...(Outputs)) != actual_fields) {
      throw std::runtime_error(
          std::format(
              "Query column count mismatch: expected {} columns, got {}.",
              sizeof...(Outputs),
              actual_fields));
    }
    int col = 0;
    (_read_column(_current_row, col++, outputs), ...);
  }

private:
  static std::string _to_prepared_name(std::string_view query_str);

  void _clear_result();

  void _read_column(int row, int col, std::string_view& out);
  void _read_column(int row, int col, std::string& out);
  void _read_column(int row, int col, int32_t& out);
  void _read_column(int row, int col, int64_t& out);
  void _read_column(int row, int col, double& out);
  void _read_column(int row, int col, bool& out);
  void
  _read_column(int row, int col, std::chrono::system_clock::time_point& out);

  template <typename T>
  void _read_column(int row, int col, std::optional<T>& out) {
    if (PQgetisnull(_res, row, col)) {
      out = std::nullopt;
      return;
    }
    _read_column(row, col, out.emplace());
  }

  PGconn* _conn = nullptr;
  bool _is_prepared = false;
  std::string _name_or_sql;
  PGresult* _res = nullptr;
  int _current_row = -1;
  int _total_rows = 0;
  param_binder _params;
};

/**
 * @brief Executes a query and reads its single expected result row.
 *
 * @throws std::runtime_error if zero or more than one row is returned.
 */
template <typename... Outputs>
void execute_read(query& active_query, Outputs&... outputs) {
  if (!active_query.step()) {
    throw std::runtime_error("No row returned from query.");
  }
  active_query.read_all(outputs...);
  if (active_query.step()) {
    throw std::runtime_error(
        "More than one row of data returned, but only one expected.");
  }
}

/**
 * @brief Executes a one-shot SQL query via PQexecParams and reads one row.
 */
template <typename... Outputs>
void execute_read(
    PGconn& conn, std::string_view query_str, Outputs&... outputs) {
  query active_query{conn, query_str};
  execute_read(active_query, outputs...);
}

} // namespace howling::pg
