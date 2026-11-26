#pragma once

#include <chrono>
#include <functional>
#include <string_view>

#include "services/db/reader/reader_db.h"
#include "services/mcp/tool_handler.h"
#include "json/value.h"

namespace howling::reader_mcp {

/**
 * @brief MCP tool handler for `get_trading_summary`.
 *
 * Thread safety: Thread-safe provided the injected clock callback (if any) is
 * thread-safe. Acquires a scoped connection from `reader_db_pool` per call.
 */
class trading_summary_handler : public mcp::tool_handler {
public:
  using clock_callback = std::function<std::chrono::system_clock::time_point()>;

  /**
   * @brief Constructs the `get_trading_summary` tool handler.
   *
   * @param db_pool
   *  Read-only database connection pool. Must outlive this handler.
   * @param clock
   *  Non-empty callback returning the current UTC time for default 24-hour
   *  window calculation. Throws `std::invalid_argument` if empty.
   */
  explicit trading_summary_handler(
      reader_db_pool& db_pool,
      clock_callback clock = std::chrono::system_clock::now);

  [[nodiscard]] std::string_view name() const override;
  [[nodiscard]] std::string_view description() const override;
  [[nodiscard]] Json::Value input_schema() const override;
  [[nodiscard]] mcp::tool_annotations annotations() const override;
  [[nodiscard]] mcp::tool_result call(const Json::Value& arguments) override;

private:
  reader_db_pool& _db_pool;
  clock_callback _clock;
};

} // namespace howling::reader_mcp
