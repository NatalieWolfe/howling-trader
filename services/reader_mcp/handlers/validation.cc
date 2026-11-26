#include "services/reader_mcp/handlers/validation.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "services/mcp/tool_handler.h"
#include "json/value.h"

namespace howling::reader_mcp {
namespace {

bool is_json_integer(const Json::Value& value) {
  return !value.isBool() && value.isIntegral() && value.isInt64();
}

} // namespace

void validate_allowed_arguments(
    const Json::Value& arguments,
    std::initializer_list<std::string_view> allowed_keys) {
  if (!arguments.isObject()) {
    throw mcp::tool_error("Tool arguments must be a JSON object.");
  }
  for (const std::string& key : arguments.getMemberNames()) {
    const bool is_allowed =
        std::find(allowed_keys.begin(), allowed_keys.end(), key) !=
        allowed_keys.end();
    if (!is_allowed) {
      throw mcp::tool_error("Unknown argument '" + key + "'.");
    }
  }
}

int32_t parse_symbol(std::string_view symbol_name) {
  howling::stock::Symbol parsed = howling::stock::SYMBOL_UNSPECIFIED;
  if (!howling::stock::Symbol_Parse(std::string(symbol_name), &parsed) ||
      parsed == howling::stock::SYMBOL_UNSPECIFIED) {
    throw mcp::tool_error(
        "Unknown symbol: \"" + std::string(symbol_name) + "\".");
  }
  return static_cast<int32_t>(parsed);
}

std::string symbol_name_from_id(int32_t symbol_id) {
  if (howling::stock::Symbol_IsValid(symbol_id) &&
      symbol_id != howling::stock::SYMBOL_UNSPECIFIED) {
    return howling::stock::Symbol_Name(
        static_cast<howling::stock::Symbol>(symbol_id));
  }
  return "UNKNOWN";
}

int32_t parse_action(std::string_view action_name) {
  howling::trading::Action parsed = howling::trading::ACTION_UNSPECIFIED;
  if (!howling::trading::Action_Parse(std::string(action_name), &parsed) ||
      parsed == howling::trading::ACTION_UNSPECIFIED) {
    throw mcp::tool_error(
        "Unknown action: \"" + std::string(action_name) +
        "\". Expected one of: BUY, SELL, HOLD.");
  }
  return static_cast<int32_t>(parsed);
}

std::string action_name_from_id(int32_t action_id) {
  if (howling::trading::Action_IsValid(action_id) &&
      action_id != howling::trading::ACTION_UNSPECIFIED) {
    return howling::trading::Action_Name(
        static_cast<howling::trading::Action>(action_id));
  }
  return "UNKNOWN";
}

std::string parse_required_string(
    const Json::Value& arguments, std::string_view field_name) {
  const std::string key(field_name);
  if (!arguments.isMember(key) || arguments[key].isNull()) {
    throw mcp::tool_error("Missing required argument '" + key + "'.");
  }
  if (!arguments[key].isString()) {
    throw mcp::tool_error("Argument '" + key + "' must be a string.");
  }
  return arguments[key].asString();
}

std::optional<std::string> parse_optional_string(
    const Json::Value& arguments, std::string_view field_name) {
  const std::string key(field_name);
  if (!arguments.isMember(key) || arguments[key].isNull()) {
    return std::nullopt;
  }
  if (!arguments[key].isString()) {
    throw mcp::tool_error("Argument '" + key + "' must be a string.");
  }
  return arguments[key].asString();
}

std::optional<bool>
parse_optional_bool(const Json::Value& arguments, std::string_view field_name) {
  const std::string key(field_name);
  if (!arguments.isMember(key) || arguments[key].isNull()) {
    return std::nullopt;
  }
  if (!arguments[key].isBool()) {
    throw mcp::tool_error("Argument '" + key + "' must be a boolean.");
  }
  return arguments[key].asBool();
}

std::optional<int64_t> parse_optional_positive_int64(
    const Json::Value& arguments, std::string_view field_name) {
  const std::string key(field_name);
  if (!arguments.isMember(key) || arguments[key].isNull()) {
    return std::nullopt;
  }
  if (!is_json_integer(arguments[key])) {
    throw mcp::tool_error("Argument '" + key + "' must be an integer.");
  }
  const int64_t value = arguments[key].asInt64();
  if (value <= 0) {
    throw mcp::tool_error("Argument '" + key + "' must be greater than zero.");
  }
  return value;
}

pagination_params parse_pagination(const Json::Value& arguments) {
  pagination_params result;

  if (arguments.isMember("limit") && !arguments["limit"].isNull()) {
    if (!is_json_integer(arguments["limit"])) {
      throw mcp::tool_error("Argument 'limit' must be an integer.");
    }
    const int64_t limit = arguments["limit"].asInt64();
    if (limit <= 0 || limit > MAX_LIMIT) {
      throw mcp::tool_error(
          "Argument 'limit' must be between 1 and " +
          std::to_string(MAX_LIMIT) + ".");
    }
    result.limit = static_cast<int32_t>(limit);
  }

  if (arguments.isMember("offset") && !arguments["offset"].isNull()) {
    if (!is_json_integer(arguments["offset"])) {
      throw mcp::tool_error("Argument 'offset' must be an integer.");
    }
    const int64_t offset = arguments["offset"].asInt64();
    if (offset < 0 || offset > std::numeric_limits<int32_t>::max()) {
      throw mcp::tool_error(
          "Argument 'offset' must be a non-negative 32-bit integer.");
    }
    result.offset = static_cast<int32_t>(offset);
  }

  return result;
}

sort_order
parse_sort_order(const Json::Value& arguments, sort_order default_order) {
  const std::optional<std::string> order_text =
      parse_optional_string(arguments, "order");
  if (!order_text.has_value()) return default_order;
  if (*order_text == "asc") return sort_order::ASC;
  if (*order_text == "desc") return sort_order::DESC;
  throw mcp::tool_error(
      "Invalid order: \"" + *order_text + "\". Expected 'asc' or 'desc'.");
}

std::chrono::system_clock::time_point
parse_iso8601_utc(std::string_view value) {
  if (value.empty()) throw mcp::tool_error("Timestamp must not be empty.");
  absl::Time parsed;
  std::string error;
  if (!absl::ParseTime(absl::RFC3339_full, value, &parsed, &error)) {
    throw mcp::tool_error(
        "Invalid ISO-8601 timestamp \"" + std::string(value) + "\": " + error);
  }
  const absl::Time min_supported = absl::FromCivil(
      absl::CivilSecond(1970, 1, 1, 0, 0, 0), absl::UTCTimeZone());
  const absl::Time max_supported = absl::FromCivil(
      absl::CivilSecond(2200, 1, 1, 0, 0, 0), absl::UTCTimeZone());
  if (parsed < min_supported || parsed > max_supported) {
    throw mcp::tool_error(
        "Timestamp \"" + std::string(value) +
        "\" is outside the supported range "
        "[1970-01-01T00:00:00Z, 2200-01-01T00:00:00Z].");
  }
  return absl::ToChronoTime(parsed);
}

std::string
format_utc_timestamp(std::chrono::system_clock::time_point timestamp) {
  const absl::Time absl_time = absl::FromChrono(timestamp);
  return absl::FormatTime(
      "%Y-%m-%dT%H:%M:%E*SZ", absl_time, absl::UTCTimeZone());
}

time_range parse_time_range(const Json::Value& arguments) {
  time_range range;
  if (const std::optional<std::string> start_time_text =
          parse_optional_string(arguments, "start_time");
      start_time_text.has_value()) {
    range.start_time = parse_iso8601_utc(*start_time_text);
  }
  if (const std::optional<std::string> end_time_text =
          parse_optional_string(arguments, "end_time");
      end_time_text.has_value()) {
    range.end_time = parse_iso8601_utc(*end_time_text);
  }
  if (range.start_time.has_value() && range.end_time.has_value() &&
      *range.start_time > *range.end_time) {
    throw mcp::tool_error("start_time must be less than or equal to end_time.");
  }
  return range;
}

Json::Value to_json_timestamp_or_null(
    const std::optional<std::chrono::system_clock::time_point>& timestamp) {
  if (!timestamp.has_value()) return Json::Value(Json::nullValue);
  return Json::Value(format_utc_timestamp(*timestamp));
}

Json::Value to_json_double_or_null(const std::optional<double>& value) {
  if (!value.has_value()) return Json::Value(Json::nullValue);
  return Json::Value(*value);
}

void add_symbol_schema(Json::Value& properties, std::string_view description) {
  Json::Value symbol(Json::objectValue);
  symbol["type"] = "string";
  symbol["description"] = std::string(description);
  properties["symbol"] = std::move(symbol);
}

void add_dry_run_schema(Json::Value& properties) {
  Json::Value dry_run(Json::objectValue);
  dry_run["type"] = "boolean";
  dry_run["description"] = "Optional filter for dry-run vs. live trades.";
  properties["dry_run"] = std::move(dry_run);
}

void add_time_range_schema(
    Json::Value& properties,
    std::string_view start_time_suffix,
    std::string_view end_time_suffix) {
  std::string start_description =
      "Inclusive start timestamp in ISO-8601 format with a required timezone "
      "offset (Z or +/-HH:MM).";
  if (!start_time_suffix.empty()) {
    start_description.push_back(' ');
    start_description.append(start_time_suffix);
  }
  Json::Value start_time(Json::objectValue);
  start_time["type"] = "string";
  start_time["format"] = "date-time";
  start_time["description"] = std::move(start_description);
  properties["start_time"] = std::move(start_time);

  std::string end_description =
      "Inclusive end timestamp in ISO-8601 format with a required timezone "
      "offset (Z or +/-HH:MM).";
  if (!end_time_suffix.empty()) {
    end_description.push_back(' ');
    end_description.append(end_time_suffix);
  }
  Json::Value end_time(Json::objectValue);
  end_time["type"] = "string";
  end_time["format"] = "date-time";
  end_time["description"] = std::move(end_description);
  properties["end_time"] = std::move(end_time);
}

void add_pagination_schema(Json::Value& properties) {
  Json::Value limit(Json::objectValue);
  limit["type"] = "integer";
  limit["minimum"] = 1;
  limit["maximum"] = MAX_LIMIT;
  limit["default"] = DEFAULT_LIMIT;
  limit["description"] =
      "Maximum number of rows to return (1-" + std::to_string(MAX_LIMIT) + ").";
  properties["limit"] = std::move(limit);

  Json::Value offset(Json::objectValue);
  offset["type"] = "integer";
  offset["minimum"] = 0;
  offset["default"] = 0;
  offset["description"] = "Number of rows to skip for pagination.";
  properties["offset"] = std::move(offset);

  Json::Value order(Json::objectValue);
  order["type"] = "string";
  order["enum"] = Json::Value(Json::arrayValue);
  order["enum"].append("asc");
  order["enum"].append("desc");
  order["default"] = "desc";
  order["description"] = "Sort order by timestamp ('asc' or 'desc').";
  properties["order"] = std::move(order);
}

Json::Value make_tool_input_schema(
    Json::Value properties,
    std::initializer_list<std::string_view> required_keys) {
  Json::Value schema(Json::objectValue);
  schema["type"] = "object";
  schema["properties"] = std::move(properties);
  schema["additionalProperties"] = false;
  if (required_keys.size() > 0) {
    Json::Value required(Json::arrayValue);
    for (std::string_view key : required_keys) {
      required.append(std::string(key));
    }
    schema["required"] = std::move(required);
  }
  return schema;
}

} // namespace howling::reader_mcp
