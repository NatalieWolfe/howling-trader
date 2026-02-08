#include "strings/json.h"

#include <memory>
#include <stdexcept>
#include <string_view>

#include "gtest/gtest.h"

namespace howling {
namespace {

TEST(ToJson, ParsesValidJson) {
  std::string_view json_str =
      R"json({"key": "value", "number": 123, "bool": true})json";
  Json::Value root = to_json(json_str);

  EXPECT_EQ(root["key"].asString(), "value");
  EXPECT_EQ(root["number"].asInt(), 123);
  EXPECT_TRUE(root["bool"].asBool());
}

TEST(ToJson, ParsesNestedJson) {
  std::string_view json_str = R"json({"nested": {"inner": "val"}})json";
  Json::Value root = to_json(json_str);

  EXPECT_EQ(root["nested"]["inner"].asString(), "val");
}

TEST(ToJson, ParsesJsonArray) {
  std::string_view json_str = R"json([1, 2, 3])json";
  Json::Value root = to_json(json_str);

  ASSERT_TRUE(root.isArray());
  EXPECT_EQ(root.size(), 3);
  EXPECT_EQ(root[0].asInt(), 1);
  EXPECT_EQ(root[2].asInt(), 3);
}

TEST(ToJson, ThrowsOnInvalidJson) {
  std::string_view invalid_json = R"({"key": "value")"; // Missing closing brace
  EXPECT_THROW(to_json(invalid_json), std::runtime_error);
}

TEST(ToJson, ThrowsOnEmptyString) {
  EXPECT_THROW(to_json(""), std::runtime_error);
}

} // namespace
} // namespace howling
