#include "strings/format.h"

#include <string>

#include "gtest/gtest.h"

namespace howling {
namespace {

TEST(EscapeMarkdownV2, NoSpecialChars) {
  EXPECT_EQ(escape_markdown_v2("Hello World"), "Hello World");
  EXPECT_EQ(escape_markdown_v2("12345"), "12345");
}

TEST(EscapeMarkdownV2, SpecialChars) {
  // Telegram MarkdownV2 reserved characters:
  // _ * [ ] ( ) ~ ` > # + - = | { } . !
  EXPECT_EQ(escape_markdown_v2("_"), R"(\_)");
  EXPECT_EQ(escape_markdown_v2("*"), R"(\*)");
  EXPECT_EQ(escape_markdown_v2("["), R"(\[)");
  EXPECT_EQ(escape_markdown_v2("]"), R"(\])");
  EXPECT_EQ(escape_markdown_v2("("), R"(\()");
  EXPECT_EQ(escape_markdown_v2(")"), R"(\))");
  EXPECT_EQ(escape_markdown_v2("~"), R"(\~)");
  EXPECT_EQ(escape_markdown_v2("`"), R"(\`)");
  EXPECT_EQ(escape_markdown_v2("."), R"(\.)");
  EXPECT_EQ(escape_markdown_v2("-"), R"(\-)");
}

TEST(EscapeMarkdownV2, FullSet) {
  std::string special = "_*[]()~`>#+-=|{}.!";
  std::string expected = R"(\_\*\[\]\(\)\~\`\>\#\+\-\=\|\{\}\.\!)";
  EXPECT_EQ(escape_markdown_v2(special), expected);
}

TEST(EscapeMarkdownV2, Mixed) {
  std::string input = "Check out: https://howling-oauth.wolfe.dev/callback";
  std::string expected =
      R"(Check out: https://howling\-oauth\.wolfe\.dev/callback)";
  EXPECT_EQ(escape_markdown_v2(input), expected);
}

} // namespace
} // namespace howling
