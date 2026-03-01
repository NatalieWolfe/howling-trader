#include "strings/format.h"

#include <chrono>
#include <format>
#include <string>
#include <string_view>

namespace howling {

std::string to_string(std::chrono::system_clock::time_point time) {
  return std::format("{:%F %T}", time);
}

std::string escape_markdown_v2(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '_':
      case '*':
      case '[':
      case ']':
      case '(':
      case ')':
      case '~':
      case '`':
      case '>':
      case '#':
      case '+':
      case '-':
      case '=':
      case '|':
      case '{':
      case '}':
      case '.':
      case '!':
        escaped += '\\';
        escaped += c;
        break;
      default:
        escaped += c;
    }
  }
  return escaped;
}

} // namespace howling
