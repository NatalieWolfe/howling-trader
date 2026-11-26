#include "services/reader_mcp/handlers/schema_resource_handler.h"

#include <string>
#include <string_view>
#include <utility>

#include "services/db/reader/reader_db.h"
#include "strings/json.h"
#include "json/value.h"

namespace howling::reader_mcp {

schema_resource_handler::schema_resource_handler(reader_db_pool& db_pool)
    : _db_pool(db_pool) {}

std::string_view schema_resource_handler::uri() const {
  return "howling://schema";
}

std::string_view schema_resource_handler::name() const {
  return "schema";
}

std::string_view schema_resource_handler::description() const {
  return "Database schema version and allowlisted table and view column "
         "definitions.";
}

std::string_view schema_resource_handler::mime_type() const {
  return "application/json";
}

std::string schema_resource_handler::read() {
  const reader_db_pool::scoped_resource db = _db_pool.acquire();
  const schema_metadata metadata = db->read_schema_metadata();

  Json::Value tables(Json::arrayValue);
  for (const table_metadata& table : metadata.tables) {
    Json::Value columns(Json::arrayValue);
    for (const column_metadata& column : table.columns) {
      Json::Value column_json(Json::objectValue);
      column_json["name"] = column.column_name;
      column_json["data_type"] = column.data_type;
      column_json["is_nullable"] = column.is_nullable;
      columns.append(std::move(column_json));
    }

    Json::Value table_json(Json::objectValue);
    table_json["name"] = table.table_name;
    table_json["columns"] = std::move(columns);
    tables.append(std::move(table_json));
  }

  Json::Value payload(Json::objectValue);
  payload["schema_version"] = metadata.schema_version;
  payload["tables"] = std::move(tables);
  return to_string(payload);
}

} // namespace howling::reader_mcp
