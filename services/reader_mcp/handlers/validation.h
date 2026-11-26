#pragma once

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

#include "services/db/reader/reader_db.h"
#include "json/value.h"

namespace howling::reader_mcp {

inline constexpr int32_t DEFAULT_LIMIT = 100;
inline constexpr int32_t MAX_LIMIT = 1000;
inline constexpr int64_t MAX_BUCKET_SECONDS = 86400 * 366;
inline constexpr std::chrono::hours DEFAULT_SUMMARY_WINDOW{24};

/**
 * @brief Validated pagination arguments for paginated MCP reader tools.
 */
struct pagination_params {
  int32_t limit = DEFAULT_LIMIT;
  int32_t offset = 0;
};

/**
 * @brief Validated optional inclusive UTC time range.
 */
struct time_range {
  std::optional<std::chrono::system_clock::time_point> start_time;
  std::optional<std::chrono::system_clock::time_point> end_time;
};

/**
 * @brief Validates that `arguments` is a JSON object with only allowed keys.
 *
 * Throws `howling::mcp::tool_error` if `arguments` is not a JSON object or
 * contains any member key not present in `allowed_keys`.
 */
void validate_allowed_arguments(
    const Json::Value& arguments,
    std::initializer_list<std::string_view> allowed_keys);

/**
 * @brief Parses a stock symbol name (e.g. "NVDA") into its numeric enum ID.
 *
 * Throws `howling::mcp::tool_error` if the symbol is unknown or unspecified.
 */
[[nodiscard]] int32_t parse_symbol(std::string_view symbol_name);

/**
 * @brief Resolves a numeric stock symbol ID back to its canonical name.
 */
[[nodiscard]] std::string symbol_name_from_id(int32_t symbol_id);

/**
 * @brief Parses a trade action name ("BUY", "SELL", "HOLD") into its enum ID.
 *
 * Throws `howling::mcp::tool_error` if the action is unknown or unspecified.
 */
[[nodiscard]] int32_t parse_action(std::string_view action_name);

/**
 * @brief Resolves a numeric trade action ID back to its canonical name.
 */
[[nodiscard]] std::string action_name_from_id(int32_t action_id);

/**
 * @brief Extracts a required string field from a JSON arguments object.
 *
 * Throws `howling::mcp::tool_error` if missing, null, or not a string.
 */
[[nodiscard]] std::string parse_required_string(
    const Json::Value& arguments, std::string_view field_name);

/**
 * @brief Extracts an optional string field from a JSON arguments object.
 *
 * Returns `std::nullopt` if absent or null. Throws `howling::mcp::tool_error`
 * if present and not a string.
 */
[[nodiscard]] std::optional<std::string> parse_optional_string(
    const Json::Value& arguments, std::string_view field_name);

/**
 * @brief Extracts an optional boolean field from a JSON arguments object.
 *
 * Returns `std::nullopt` if absent or null. Throws `howling::mcp::tool_error`
 * if present and not a boolean.
 */
[[nodiscard]] std::optional<bool>
parse_optional_bool(const Json::Value& arguments, std::string_view field_name);

/**
 * @brief Extracts an optional strictly positive int64 field.
 *
 * Returns `std::nullopt` if absent or null. Throws `howling::mcp::tool_error`
 * if present and not a positive signed 64-bit integer.
 */
[[nodiscard]] std::optional<int64_t> parse_optional_positive_int64(
    const Json::Value& arguments, std::string_view field_name);

/**
 * @brief Parses optional `limit` and `offset` pagination fields.
 *
 * Enforces `1 <= limit <= MAX_LIMIT` and `offset >= 0`. Throws
 * `howling::mcp::tool_error` on invalid types or out-of-range values.
 */
[[nodiscard]] pagination_params parse_pagination(const Json::Value& arguments);

/**
 * @brief Parses an optional `order` field ("asc" or "desc").
 *
 * Throws `howling::mcp::tool_error` on invalid types or unknown values.
 */
[[nodiscard]] sort_order parse_sort_order(
    const Json::Value& arguments, sort_order default_order = sort_order::DESC);

/**
 * @brief Parses an ISO-8601 / RFC-3339 timestamp with timezone offset to UTC.
 *
 * Accepts `Z`, `+HH:MM`, `-HH:MM`, and optional fractional seconds within the
 * supported range `[1970-01-01T00:00:00Z, 2200-01-01T00:00:00Z]`. Throws
 * `howling::mcp::tool_error` if the timestamp is invalid, lacks a timezone, or
 * is out of range.
 */
[[nodiscard]] std::chrono::system_clock::time_point
parse_iso8601_utc(std::string_view value);

/**
 * @brief Formats a UTC time_point as an ISO-8601 UTC string ending in `Z`.
 */
[[nodiscard]] std::string
format_utc_timestamp(std::chrono::system_clock::time_point timestamp);

/**
 * @brief Parses optional `start_time` and `end_time` fields and validates
 * `start_time <= end_time`.
 */
[[nodiscard]] time_range parse_time_range(const Json::Value& arguments);

/**
 * @brief Converts an optional timestamp to a UTC JSON string or JSON null.
 */
[[nodiscard]] Json::Value to_json_timestamp_or_null(
    const std::optional<std::chrono::system_clock::time_point>& timestamp);

/**
 * @brief Converts an optional double to a JSON number or JSON null.
 */
[[nodiscard]] Json::Value
to_json_double_or_null(const std::optional<double>& value);

/**
 * @brief Adds a `symbol` property definition to a JSON Schema properties map.
 */
void add_symbol_schema(Json::Value& properties, std::string_view description);

/**
 * @brief Adds a `dry_run` property definition to a JSON Schema properties map.
 */
void add_dry_run_schema(Json::Value& properties);

/**
 * @brief Adds `start_time` and `end_time` RFC-3339 property definitions to a
 * JSON Schema properties map.
 *
 * @param properties
 *  Target JSON Schema `properties` object to populate.
 * @param start_time_suffix
 *  Optional additional description sentence appended to `start_time`.
 * @param end_time_suffix
 *  Optional additional description sentence appended to `end_time`.
 */
void add_time_range_schema(
    Json::Value& properties,
    std::string_view start_time_suffix = "",
    std::string_view end_time_suffix = "");

/**
 * @brief Adds `limit`, `offset`, and `order` pagination property definitions
 * to a JSON Schema properties map.
 */
void add_pagination_schema(Json::Value& properties);

/**
 * @brief Builds a strict object JSON Schema with `additionalProperties: false`.
 */
[[nodiscard]] Json::Value make_tool_input_schema(
    Json::Value properties,
    std::initializer_list<std::string_view> required_keys = {});

} // namespace howling::reader_mcp
