#include "services/db/schema/schema.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <generator>
#include <sstream>
#include <string>
#include <string_view>

#include "environment/runfiles.h"
#include "strings/parse.h"
#include "strings/trim.h"

namespace howling::db_internal {
namespace {

namespace fs = ::std::filesystem;

constexpr int DEFAULT_SCHEMA_VERSION = 1;
const fs::path SCHEMA_DIR = "howling-trader/services/db/schema";

fs::path full_schema_file() {
  fs::path full_schema_path = SCHEMA_DIR / "full.sql";
  return runfile(full_schema_path.string());
}

std::string load_full_schema() {
  fs::path schema_path = full_schema_file();
  if (!fs::exists(schema_path)) {
    throw std::runtime_error("Full schema file missing!");
  }
  std::stringstream schema_stream;
  schema_stream << std::ifstream(schema_path).rdbuf();
  return std::move(schema_stream).str();
}

} // namespace

int get_schema_version() {
  constexpr std::string_view UPDATE_PREFIX = "update_";
  int max_version = DEFAULT_SCHEMA_VERSION;
  for (fs::directory_entry entry :
       fs::directory_iterator(runfile(SCHEMA_DIR.string()))) {
    std::string filename = entry.path().stem().string();
    if (filename.starts_with(UPDATE_PREFIX)) {
      max_version = std::max(
          max_version,
          parse_int(
              std::string_view{
                  filename.begin() + UPDATE_PREFIX.length(), filename.end()}));
    }
  }
  return max_version;
}

std::generator<std::string_view> get_full_schema() {
  std::string schema = load_full_schema();
  if (schema.empty()) {
    throw std::runtime_error("Full schema file is empty!");
  }

  constexpr std::string_view COMMAND_BREAK = ";\n\n";
  size_t pos, offset = 0;
  while ((pos = schema.find(COMMAND_BREAK, offset)) != std::string::npos) {
    co_yield trim(std::string_view{schema.data() + offset, pos - offset + 1});
    offset = pos + 1;
  }
  if (schema.length() > (offset + COMMAND_BREAK.length())) {
    co_yield trim(
        std::string_view{schema.data() + offset, schema.length() - offset});
  }
}

} // namespace howling::db_internal
