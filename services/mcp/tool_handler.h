#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "json/value.h"

namespace howling::mcp {

/**
 * @brief Optional MCP behavioral hints for a registered tool.
 */
struct tool_annotations {
  std::optional<bool> read_only_hint;
  std::optional<bool> destructive_hint;
  std::optional<bool> idempotent_hint;
  std::optional<bool> open_world_hint;

  /**
   * @brief Serializes populated annotation hints to an MCP JSON object.
   */
  [[nodiscard]] Json::Value to_json() const {
    Json::Value json(Json::objectValue);
    if (read_only_hint.has_value()) json["readOnlyHint"] = *read_only_hint;
    if (destructive_hint.has_value()) {
      json["destructiveHint"] = *destructive_hint;
    }
    if (idempotent_hint.has_value()) {
      json["idempotentHint"] = *idempotent_hint;
    }
    if (open_world_hint.has_value()) {
      json["openWorldHint"] = *open_world_hint;
    }
    return json;
  }
};

/**
 * @brief Result returned by a tool invocation.
 */
struct tool_result {
  std::string text;
  bool is_error = false;

  /**
   * @brief Creates a successful tool result with text content.
   */
  [[nodiscard]] static tool_result ok(std::string content_text) {
    return tool_result{.text = std::move(content_text), .is_error = false};
  }

  /**
   * @brief Creates an error tool result with text content.
   */
  [[nodiscard]] static tool_result error(std::string error_text) {
    return tool_result{.text = std::move(error_text), .is_error = true};
  }
};

/**
 * @brief Exception thrown by a tool handler to return an MCP isError result.
 */
class tool_error : public std::runtime_error {
public:
  explicit tool_error(const std::string& message)
      : std::runtime_error(message) {}

  explicit tool_error(const char* message) : std::runtime_error(message) {}
};

/**
 * @brief Base interface for an MCP tool handler.
 *
 * Implementations registered with an `mcp_dispatcher` must be thread-safe when
 * `mcp_dispatcher::dispatch()` or `mcp_dispatcher::dispatch_request()` is
 * called concurrently from multiple threads, as `call()` and metadata accessors
 * may be invoked concurrently.
 */
class tool_handler {
public:
  virtual ~tool_handler() = default;

  /**
   * @brief Returns the unique tool name exposed via `tools/list`.
   */
  [[nodiscard]] virtual std::string_view name() const = 0;

  /**
   * @brief Returns a human-readable description of the tool.
   */
  [[nodiscard]] virtual std::string_view description() const = 0;

  /**
   * @brief Returns the JSON Schema object describing accepted arguments.
   */
  [[nodiscard]] virtual Json::Value input_schema() const = 0;

  /**
   * @brief Returns optional behavioral annotations for the tool.
   */
  [[nodiscard]] virtual tool_annotations annotations() const { return {}; }

  /**
   * @brief Executes the tool with validated JSON object arguments.
   *
   * Throwing `tool_error`, `std::invalid_argument`, or any `std::exception`
   * causes the dispatcher to return a tool result with `isError: true`.
   */
  [[nodiscard]] virtual tool_result call(const Json::Value& arguments) = 0;
};

} // namespace howling::mcp
