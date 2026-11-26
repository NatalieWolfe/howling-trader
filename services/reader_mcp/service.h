#pragma once

#include <string_view>

#include "services/db/reader/reader_db.h"
#include "services/mcp/dispatcher.h"

namespace howling::reader_mcp {

inline constexpr std::string_view SERVER_NAME = "howling-reader-mcp";
inline constexpr std::string_view SERVER_VERSION = "1.0.0";

/**
 * @brief Owns the MCP dispatcher and registers all read-only trading database
 * tools and resources.
 *
 * Thread safety: Thread-safe after construction. All registered handlers are
 * thread-safe and share the injected `reader_db_pool`.
 */
class reader_mcp_service {
public:
  /**
   * @brief Constructs the service and registers all reader tools and resources.
   *
   * @param db_pool
   *  Thread-safe pool of read-only database connections. Must outlive this
   *  service instance.
   */
  explicit reader_mcp_service(reader_db_pool& db_pool);

  reader_mcp_service(const reader_mcp_service&) = delete;
  reader_mcp_service& operator=(const reader_mcp_service&) = delete;

  /**
   * @brief Returns the configured MCP JSON-RPC dispatcher.
   */
  [[nodiscard]] const mcp::mcp_dispatcher& dispatcher() const noexcept {
    return _dispatcher;
  }

  /**
   * @brief Dispatches a raw JSON-RPC 2.0 request payload.
   */
  [[nodiscard]] mcp::dispatch_result dispatch(std::string_view raw_json) const {
    return _dispatcher.dispatch(raw_json);
  }

private:
  mcp::mcp_dispatcher _dispatcher;
};

} // namespace howling::reader_mcp
