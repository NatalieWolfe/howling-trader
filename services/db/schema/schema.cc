#include "services/db/schema/schema.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
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

std::generator<std::string> split_commands(std::string schema) {
  size_t offset = 0;
  while (offset < schema.length()) {
    size_t pos = schema.find(';', offset);
    if (pos == std::string::npos) {
      std::string_view tail = trim(std::string_view{schema}.substr(offset));
      if (!tail.empty()) co_yield std::string{tail};
      break;
    }
    std::string_view cmd =
        trim(std::string_view{schema}.substr(offset, pos - offset + 1));
    if (!cmd.empty()) co_yield std::string{cmd};
    offset = pos + 1;
  }
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
                  filename.data() + UPDATE_PREFIX.length(),
                  filename.length() - UPDATE_PREFIX.length()}));
    }
  }
  return max_version;
}

std::generator<std::string> get_full_schema() {
  std::string schema = load_full_schema();
  if (schema.empty()) {
    throw std::runtime_error("Full schema file is empty!");
  }
  return split_commands(std::move(schema));
}

std::generator<std::string> get_schema_update(int from_version) {
  int target_version = get_schema_version();
  for (int v = from_version + 1; v <= target_version; ++v) {
    fs::path update_path =
        runfile((SCHEMA_DIR / std::format("update_{}.sql", v)).string());
    if (!fs::exists(update_path)) continue;

    std::ifstream update_stream(update_path);
    std::string update_sql(
        (std::istreambuf_iterator<char>(update_stream)),
        (std::istreambuf_iterator<char>()));

    for (std::string command : split_commands(std::move(update_sql))) {
      co_yield command;
    }
  }
}

} // namespace howling::db_internal
