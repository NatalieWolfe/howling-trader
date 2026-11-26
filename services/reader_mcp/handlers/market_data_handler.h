#pragma once

#include <string_view>

#include "services/db/reader/reader_db.h"
#include "services/mcp/tool_handler.h"
#include "json/value.h"

namespace howling::reader_mcp {

/**
 * @brief MCP tool handler for `get_market_data`.
 *
 * Thread safety: Thread-safe. Acquires a scoped connection from the injected
 * `reader_db_pool` per call.
 */
class market_data_handler : public mcp::tool_handler {
public:
  /**
   * @brief Constructs the `get_market_data` tool handler.
   *
   * @param db_pool
   *  Read-only database connection pool. Must outlive this handler.
   */
  explicit market_data_handler(reader_db_pool& db_pool);

  [[nodiscard]] std::string_view name() const override;
  [[nodiscard]] std::string_view description() const override;
  [[nodiscard]] Json::Value input_schema() const override;
  [[nodiscard]] mcp::tool_annotations annotations() const override;
  [[nodiscard]] mcp::tool_result call(const Json::Value& arguments) override;

private:
  reader_db_pool& _db_pool;
};

} // namespace howling::reader_mcp
