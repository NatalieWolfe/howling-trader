#include "services/db/pg/query.h"

#include <functional>

namespace howling::pg {
namespace {

constexpr std::string_view PREPARED_PREFIX = "prepared_";

} // namespace

void param_binder::clear() {
  _buffers.clear();
  _param_types.clear();
  _param_values.clear();
  _param_lengths.clear();
  _param_formats.clear();
}

void param_binder::reserve(size_t count) {
  _param_types.reserve(count);
  _param_values.reserve(count);
  _param_lengths.reserve(count);
  _param_formats.reserve(count);
}

void param_binder::_store(unsigned int oid, std::string encoded) {
  std::string& buffer = _buffers.emplace_back(std::move(encoded));
  _param_types.push_back(oid);
  _param_values.push_back(buffer.data());
  _param_lengths.push_back(static_cast<int>(buffer.size()));
  _param_formats.push_back(BINARY_FORMAT);
}

void param_binder::bind(int32_t value) {
  _store(INT4_OID, encode_int32(value));
}

void param_binder::bind(int64_t value) {
  _store(INT8_OID, encode_int64(value));
}

void param_binder::bind(double value) {
  _store(FLOAT8_OID, encode_double(value));
}

void param_binder::bind(bool value) {
  _store(BOOL_OID, encode_bool(value));
}

void param_binder::bind(const char* value) {
  if (value == nullptr) {
    throw std::invalid_argument("Cannot bind null C-string pointer.");
  }
  bind(std::string_view{value});
}

void param_binder::bind(std::string_view value) {
  _store(TEXT_OID, std::string{value});
}

void param_binder::bind(const std::string& value) {
  _store(TEXT_OID, value);
}

void param_binder::bind(const bytes& value) {
  _store(BYTEA_OID, value.value);
}

void param_binder::bind(std::chrono::system_clock::time_point value) {
  _store(TIMESTAMP_OID, encode_timestamp(value));
}

// MARK: Query

query::query(prepared_tag_t, PGconn* conn, std::string name)
    : _conn{conn}, _is_prepared{true}, _name_or_sql{std::move(name)} {}

query::query(PGconn& conn, std::string_view query_str)
    : _conn{&conn}, _is_prepared{false}, _name_or_sql{query_str} {}

query::~query() {
  _clear_result();
}

void query::_clear_result() {
  if (_res != nullptr) {
    PQclear(_res);
    _res = nullptr;
  }
  _current_row = -1;
  _total_rows = 0;
}

void query::execute() {
  _clear_result();
  if (_is_prepared) {
    _res = PQexecPrepared(
        _conn,
        _name_or_sql.c_str(),
        _params.count(),
        _params.values(),
        _params.lengths(),
        _params.formats(),
        BINARY_FORMAT);
  } else {
    _res = PQexecParams(
        _conn,
        _name_or_sql.c_str(),
        _params.count(),
        _params.types(),
        _params.values(),
        _params.lengths(),
        _params.formats(),
        BINARY_FORMAT);
  }

  int code = PQresultStatus(_res);
  if (code != PGRES_COMMAND_OK && code != PGRES_TUPLES_OK) {
    PGresult* failed = _res;
    _res = nullptr;
    check_command_result(failed, *_conn);
  }
  _current_row = -1;
  _total_rows = PQntuples(_res);
}

bool query::step() {
  if (_res == nullptr) execute();
  return ++_current_row < _total_rows;
}

std::string query::_to_prepared_name(std::string_view query_str) {
  return std::string{PREPARED_PREFIX} +
      std::to_string(std::hash<std::string_view>{}(query_str));
}

void query::_read_column(int row, int col, std::string_view& out) {
  check_not_null(_res, row, col);
  out = decode_string_view(
      PQgetvalue(_res, row, col),
      PQgetlength(_res, row, col),
      PQftype(_res, col),
      col);
}

void query::_read_column(int row, int col, std::string& out) {
  std::string_view out_view;
  _read_column(row, col, out_view);
  out = std::string{out_view};
}

void query::_read_column(int row, int col, int32_t& out) {
  check_not_null(_res, row, col);
  out = decode_int32(
      PQgetvalue(_res, row, col),
      PQgetlength(_res, row, col),
      PQftype(_res, col),
      col);
}

void query::_read_column(int row, int col, int64_t& out) {
  check_not_null(_res, row, col);
  out = decode_int64(
      PQgetvalue(_res, row, col),
      PQgetlength(_res, row, col),
      PQftype(_res, col),
      col);
}

void query::_read_column(int row, int col, double& out) {
  check_not_null(_res, row, col);
  out = decode_double(
      PQgetvalue(_res, row, col),
      PQgetlength(_res, row, col),
      PQftype(_res, col),
      col);
}

void query::_read_column(int row, int col, bool& out) {
  check_not_null(_res, row, col);
  out = decode_bool(
      PQgetvalue(_res, row, col),
      PQgetlength(_res, row, col),
      PQftype(_res, col),
      col);
}

void query::_read_column(
    int row, int col, std::chrono::system_clock::time_point& out) {
  check_not_null(_res, row, col);
  out = decode_timestamp(
      PQgetvalue(_res, row, col),
      PQgetlength(_res, row, col),
      PQftype(_res, col),
      col);
}

} // namespace howling::pg
