#pragma once

#include <string>
#include <string_view>

#include "services/db/reader/reader_db.h"
#include "services/mcp/resource_handler.h"

namespace howling::reader_mcp {

/**
 * @brief MCP resource handler for `howling://schema`.
 *
 * Thread safety: Thread-safe. Acquires a scoped connection from the injected
 * `reader_db_pool` per read.
 */
class schema_resource_handler : public mcp::resource_handler {
public:
  /**
   * @brief Constructs the `howling://schema` resource handler.
   *
   * @param db_pool
   *  Read-only database connection pool. Must outlive this handler.
   */
  explicit schema_resource_handler(reader_db_pool& db_pool);

  [[nodiscard]] std::string_view uri() const override;
  [[nodiscard]] std::string_view name() const override;
  [[nodiscard]] std::string_view description() const override;
  [[nodiscard]] std::string_view mime_type() const override;
  [[nodiscard]] std::string read() override;

private:
  reader_db_pool& _db_pool;
};

} // namespace howling::reader_mcp
