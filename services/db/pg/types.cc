#include "services/db/pg/types.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <endian.h>
#include <format>
#include <stdexcept>

namespace howling::pg {
namespace {

using ::std::chrono::duration_cast;
using ::std::chrono::microseconds;
using ::std::chrono::system_clock;

std::string format_oids(std::span<const unsigned int> oids) {
  std::string result;
  for (size_t i = 0; i < oids.size(); ++i) {
    if (i > 0) result += ", ";
    result += std::to_string(oids[i]);
  }
  return result;
}

void check_non_null_data(
    const char* data, int col, std::string_view type_name) {
  if (data == nullptr) {
    throw std::runtime_error(
        std::format(
            "Postgres column {} has null data pointer for {}.",
            col,
            type_name));
  }
}

} // namespace

void validate_column_type(
    int col,
    unsigned int actual_oid,
    int actual_length,
    std::span<const unsigned int> expected_oids,
    std::optional<int> expected_length,
    std::string_view type_name) {
  bool oid_matches =
      std::find(expected_oids.begin(), expected_oids.end(), actual_oid) !=
      expected_oids.end();
  if (!oid_matches) {
    throw std::runtime_error(
        std::format(
            "Postgres column {} type mismatch for {}: expected OID in [{}], "
            "got OID {} (length {}).",
            col,
            type_name,
            format_oids(expected_oids),
            actual_oid,
            actual_length));
  }
  if (actual_length < 0) {
    throw std::runtime_error(
        std::format(
            "Postgres column {} has negative length {} for {} (OID {}).",
            col,
            actual_length,
            type_name,
            actual_oid));
  }
  if (expected_length && actual_length != *expected_length) {
    throw std::runtime_error(
        std::format(
            "Postgres column {} length mismatch for {} (OID {}): expected {} "
            "bytes, got {} bytes.",
            col,
            type_name,
            actual_oid,
            *expected_length,
            actual_length));
  }
}

std::string encode_int32(int32_t value) {
  uint32_t net_val = htobe32(static_cast<uint32_t>(value));
  return std::string(reinterpret_cast<const char*>(&net_val), sizeof(net_val));
}

std::string encode_int64(int64_t value) {
  uint64_t net_val = htobe64(static_cast<uint64_t>(value));
  return std::string(reinterpret_cast<const char*>(&net_val), sizeof(net_val));
}

std::string encode_double(double value) {
  uint64_t net_val = htobe64(std::bit_cast<uint64_t>(value));
  return std::string(reinterpret_cast<const char*>(&net_val), sizeof(net_val));
}

std::string encode_bool(bool value) {
  char wire_val = value ? 1 : 0;
  return std::string(&wire_val, 1);
}

std::string encode_timestamp(system_clock::time_point value) {
  microseconds duration =
      duration_cast<microseconds>(value.time_since_epoch() - PG_EPOCH);
  return encode_int64(duration.count());
}

int32_t decode_int32(const char* data, int length, unsigned int oid, int col) {
  check_non_null_data(data, col, "int32_t");
  constexpr std::array<unsigned int, 1> EXPECTED_OIDS{INT4_OID};
  validate_column_type(
      col, oid, length, EXPECTED_OIDS, sizeof(int32_t), "int32_t");
  uint32_t net_val = 0;
  std::memcpy(&net_val, data, sizeof(net_val));
  return static_cast<int32_t>(be32toh(net_val));
}

int64_t decode_int64(const char* data, int length, unsigned int oid, int col) {
  check_non_null_data(data, col, "int64_t");
  constexpr std::array<unsigned int, 1> EXPECTED_OIDS{INT8_OID};
  validate_column_type(
      col, oid, length, EXPECTED_OIDS, sizeof(int64_t), "int64_t");
  uint64_t net_val = 0;
  std::memcpy(&net_val, data, sizeof(net_val));
  return static_cast<int64_t>(be64toh(net_val));
}

double decode_double(const char* data, int length, unsigned int oid, int col) {
  check_non_null_data(data, col, "double");
  constexpr std::array<unsigned int, 1> EXPECTED_OIDS{FLOAT8_OID};
  validate_column_type(
      col, oid, length, EXPECTED_OIDS, sizeof(double), "double");
  uint64_t net_val = 0;
  std::memcpy(&net_val, data, sizeof(net_val));
  return std::bit_cast<double>(be64toh(net_val));
}

bool decode_bool(const char* data, int length, unsigned int oid, int col) {
  check_non_null_data(data, col, "bool");
  constexpr std::array<unsigned int, 1> EXPECTED_OIDS{BOOL_OID};
  validate_column_type(col, oid, length, EXPECTED_OIDS, 1, "bool");
  return *data != 0;
}

system_clock::time_point
decode_timestamp(const char* data, int length, unsigned int oid, int col) {
  check_non_null_data(data, col, "time_point");
  constexpr std::array<unsigned int, 2> EXPECTED_OIDS{
      TIMESTAMP_OID, TIMESTAMPTZ_OID};
  validate_column_type(
      col, oid, length, EXPECTED_OIDS, sizeof(int64_t), "time_point");
  uint64_t net_val = 0;
  std::memcpy(&net_val, data, sizeof(net_val));
  int64_t wire_val = static_cast<int64_t>(be64toh(net_val));
  return system_clock::time_point{PG_EPOCH + microseconds{wire_val}};
}

std::string_view
decode_string_view(const char* data, int length, unsigned int oid, int col) {
  check_non_null_data(data, col, "string_view");
  constexpr std::array<unsigned int, 6> EXPECTED_OIDS{
      TEXT_OID, VARCHAR_OID, BPCHAR_OID, NAME_OID, BYTEA_OID, UNKNOWN_OID};
  validate_column_type(
      col, oid, length, EXPECTED_OIDS, std::nullopt, "string_view");
  return std::string_view{data, static_cast<size_t>(length)};
}

} // namespace howling::pg
