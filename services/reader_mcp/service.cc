#include "services/reader_mcp/service.h"

#include <memory>
#include <string>

#include "services/db/reader/reader_db.h"
#include "services/mcp/dispatcher.h"
#include "services/reader_mcp/handlers/actions_resource_handler.h"
#include "services/reader_mcp/handlers/candles_handler.h"
#include "services/reader_mcp/handlers/market_data_handler.h"
#include "services/reader_mcp/handlers/schema_resource_handler.h"
#include "services/reader_mcp/handlers/symbols_resource_handler.h"
#include "services/reader_mcp/handlers/trades_handler.h"
#include "services/reader_mcp/handlers/trading_summary_handler.h"

namespace howling::reader_mcp {

reader_mcp_service::reader_mcp_service(reader_db_pool& db_pool)
    : _dispatcher(
          mcp::server_info{
              .name = std::string(SERVER_NAME),
              .version = std::string(SERVER_VERSION)}) {
  _dispatcher.register_tool(std::make_unique<candles_handler>(db_pool));
  _dispatcher.register_tool(std::make_unique<market_data_handler>(db_pool));
  _dispatcher.register_tool(std::make_unique<trades_handler>(db_pool));
  _dispatcher.register_tool(std::make_unique<trading_summary_handler>(db_pool));

  _dispatcher.register_resource(
      std::make_unique<symbols_resource_handler>(db_pool));
  _dispatcher.register_resource(
      std::make_unique<actions_resource_handler>(db_pool));
  _dispatcher.register_resource(
      std::make_unique<schema_resource_handler>(db_pool));
}

} // namespace howling::reader_mcp
