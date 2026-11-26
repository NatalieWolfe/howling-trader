#include "services/reader_mcp/handlers/actions_resource_handler.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/db/reader/reader_db.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::reader_mcp {

actions_resource_handler::actions_resource_handler(reader_db_pool& db_pool)
    : _db_pool(db_pool) {}

std::string_view actions_resource_handler::uri() const {
  return "howling://actions";
}

std::string_view actions_resource_handler::name() const {
  return "actions";
}

std::string_view actions_resource_handler::description() const {
  return "Trade action enum definitions from enum_actions.";
}

std::string_view actions_resource_handler::mime_type() const {
  return "application/json";
}

std::string actions_resource_handler::read() {
  const reader_db_pool::scoped_resource db = _db_pool.acquire();
  const std::vector<action_info> actions_list = db->read_actions_metadata();

  Json::Value actions(Json::arrayValue);
  for (const action_info& entry : actions_list) {
    Json::Value item(Json::objectValue);
    item["id"] = entry.id;
    item["name"] = entry.name;
    actions.append(std::move(item));
  }

  Json::Value payload(Json::objectValue);
  payload["actions"] = std::move(actions);
  return to_string(payload);
}

} // namespace howling::reader_mcp
