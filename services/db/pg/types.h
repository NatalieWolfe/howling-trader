#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace howling::pg {

inline constexpr int BINARY_FORMAT = 1;
inline constexpr std::chrono::microseconds PG_EPOCH{946684800000000};

inline constexpr unsigned int BOOL_OID = 16;
inline constexpr unsigned int BYTEA_OID = 17;
inline constexpr unsigned int NAME_OID = 19;
inline constexpr unsigned int INT8_OID = 20;
inline constexpr unsigned int INT2_OID = 21;
inline constexpr unsigned int INT4_OID = 23;
inline constexpr unsigned int TEXT_OID = 25;
inline constexpr unsigned int FLOAT4_OID = 700;
inline constexpr unsigned int FLOAT8_OID = 701;
inline constexpr unsigned int UNKNOWN_OID = 705;
inline constexpr unsigned int BPCHAR_OID = 1042;
inline constexpr unsigned int VARCHAR_OID = 1043;
inline constexpr unsigned int TIMESTAMP_OID = 1114;
inline constexpr unsigned int TIMESTAMPTZ_OID = 1184;
inline constexpr unsigned int NUMERIC_OID = 1700;

/**
 * @brief Wrapper representing a raw BYTEA parameter value.
 */
struct bytes {
  std::string value;
};

/**
 * @brief Returns the PostgreSQL OID corresponding to C++ type T.
 */
template <typename T>
unsigned int pg_type_id();

template <>
inline unsigned int pg_type_id<bool>() {
  return BOOL_OID;
}

template <>
inline unsigned int pg_type_id<bytes>() {
  return BYTEA_OID;
}

template <>
inline unsigned int pg_type_id<int64_t>() {
  return INT8_OID;
}

template <>
inline unsigned int pg_type_id<int32_t>() {
  return INT4_OID;
}

template <>
inline unsigned int pg_type_id<std::string_view>() {
  return TEXT_OID;
}

template <>
inline unsigned int pg_type_id<std::string>() {
  return TEXT_OID;
}

template <>
inline unsigned int pg_type_id<double>() {
  return FLOAT8_OID;
}

template <>
inline unsigned int pg_type_id<std::chrono::system_clock::time_point>() {
  return TIMESTAMP_OID;
}

namespace internal {

template <typename T>
struct unwrap_optional {
  using type = T;
};

template <typename T>
struct unwrap_optional<std::optional<T>> {
  using type = T;
};

} // namespace internal

/**
 * @brief Returns the PostgreSQL OID for T, unwrapping cv-ref and std::optional.
 */
template <typename T>
unsigned int pg_type_id_for() {
  using decayed = std::remove_cvref_t<T>;
  using unwrapped = typename internal::unwrap_optional<decayed>::type;
  return pg_type_id<unwrapped>();
}

/**
 * @brief Validates a binary-format PostgreSQL column OID and byte length.
 *
 * @param col
 *  Zero-based column index in the result set.
 * @param actual_oid
 *  OID reported by PQftype.
 * @param actual_length
 *  Byte length reported by PQgetlength.
 * @param expected_oids
 *  List of acceptable PostgreSQL OIDs for the target C++ type.
 * @param expected_length
 *  Required byte length for fixed-width types, or std::nullopt for variable.
 * @param type_name
 *  Human-readable C++ type name used in error messages.
 *
 * @throws std::runtime_error if the OID or byte length does not match.
 */
void validate_column_type(
    int col,
    unsigned int actual_oid,
    int actual_length,
    std::span<const unsigned int> expected_oids,
    std::optional<int> expected_length,
    std::string_view type_name);

/**
 * @brief Encodes an int32_t in big-endian PostgreSQL binary wire format.
 */
[[nodiscard]] std::string encode_int32(int32_t value);

/**
 * @brief Encodes an int64_t in big-endian PostgreSQL binary wire format.
 */
[[nodiscard]] std::string encode_int64(int64_t value);

/**
 * @brief Encodes a double in big-endian IEEE 754 PostgreSQL binary wire format.
 */
[[nodiscard]] std::string encode_double(double value);

/**
 * @brief Encodes a bool in 1-byte PostgreSQL binary wire format.
 */
[[nodiscard]] std::string encode_bool(bool value);

/**
 * @brief Encodes a UTC time_point as microseconds since PG_EPOCH (2000-01-01).
 */
[[nodiscard]] std::string
encode_timestamp(std::chrono::system_clock::time_point value);

/**
 * @brief Decodes and validates a binary int32_t column value.
 */
[[nodiscard]] int32_t
decode_int32(const char* data, int length, unsigned int oid, int col = 0);

/**
 * @brief Decodes and validates a binary int64_t column value.
 */
[[nodiscard]] int64_t
decode_int64(const char* data, int length, unsigned int oid, int col = 0);

/**
 * @brief Decodes and validates a binary double (FLOAT8) column value.
 */
[[nodiscard]] double
decode_double(const char* data, int length, unsigned int oid, int col = 0);

/**
 * @brief Decodes and validates a binary bool column value.
 */
[[nodiscard]] bool
decode_bool(const char* data, int length, unsigned int oid, int col = 0);

/**
 * @brief Decodes and validates a binary TIMESTAMP/TIMESTAMPTZ column value.
 */
[[nodiscard]] std::chrono::system_clock::time_point
decode_timestamp(const char* data, int length, unsigned int oid, int col = 0);

/**
 * @brief Decodes and validates a binary text or bytea column as a string_view.
 */
[[nodiscard]] std::string_view
decode_string_view(const char* data, int length, unsigned int oid, int col = 0);

} // namespace howling::pg
