#include "strings/json.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "json/reader.h"
#include "json/writer.h"

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

std::string to_string(const Json::Value& json) {
  static const Json::StreamWriterBuilder builder = ([]() {
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    return builder;
  })();
  return Json::writeString(builder, json);
}

} // namespace howling
