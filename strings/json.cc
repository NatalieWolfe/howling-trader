#include "strings/json.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "json/reader.h"

namespace howling {

Json::Value to_json(std::string_view str) {
  static const Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader{builder.newCharReader()};
  Json::Value root;
  std::string err;
  if (!reader->parse(str.data(), str.data() + str.size(), &root, &err)) {
    throw std::runtime_error("Failed to parse JSON string: " + err);
  }
  return root;
}

} // namespace howling
