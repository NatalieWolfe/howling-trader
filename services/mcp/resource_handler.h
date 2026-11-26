#pragma once

#include <string>
#include <string_view>

namespace howling::mcp {

/**
 * @brief Base interface for an MCP resource handler.
 *
 * Implementations registered with an `mcp_dispatcher` must be thread-safe when
 * `mcp_dispatcher::dispatch()` or `mcp_dispatcher::dispatch_request()` is
 * called concurrently from multiple threads, as `read()` and metadata accessors
 * may be invoked concurrently.
 */
class resource_handler {
public:
  virtual ~resource_handler() = default;

  /**
   * @brief Returns the unique resource URI exposed via `resources/list`.
   */
  [[nodiscard]] virtual std::string_view uri() const = 0;

  /**
   * @brief Returns the human-readable resource name.
   */
  [[nodiscard]] virtual std::string_view name() const = 0;

  /**
   * @brief Returns a human-readable description of the resource.
   */
  [[nodiscard]] virtual std::string_view description() const = 0;

  /**
   * @brief Returns the MIME type of the resource content.
   */
  [[nodiscard]] virtual std::string_view mime_type() const = 0;

  /**
   * @brief Reads and returns the text content of the resource.
   */
  [[nodiscard]] virtual std::string read() = 0;
};

} // namespace howling::mcp
