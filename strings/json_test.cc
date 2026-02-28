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

TEST(ToString, SerializesObject) {
  Json::Value root;
  root["key"] = "value";
  root["number"] = 123;
  root["bool"] = true;

  std::string result = to_string(root);
  Json::Value back = to_json(result);
  EXPECT_EQ(back["key"].asString(), "value");
  EXPECT_EQ(back["number"].asInt(), 123);
  EXPECT_TRUE(back["bool"].asBool());
}

TEST(ToString, SerializesArray) {
  Json::Value root(Json::arrayValue);
  root.append(1);
  root.append("two");
  root.append(false);

  std::string result = to_string(root);
  Json::Value back = to_json(result);
  ASSERT_TRUE(back.isArray());
  EXPECT_EQ(back.size(), 3);
  EXPECT_EQ(back[0].asInt(), 1);
  EXPECT_EQ(back[1].asString(), "two");
  EXPECT_FALSE(back[2].asBool());
}

TEST(ToString, CompactSerialization) {
  Json::Value root;
  root["a"] = 1;
  root["b"] = 2;

  std::string result = to_string(root);
  EXPECT_EQ(result, "{\"a\":1,\"b\":2}");
}

} // namespace
} // namespace howling
