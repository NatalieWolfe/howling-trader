#include "strings/trim.h"

#include <string_view>

namespace howling {

constexpr std::string_view WHITESPACE_CHARS = " \t\n\r";

std::string_view trim(std::string_view term) {
  size_t start = term.find_first_not_of(WHITESPACE_CHARS);
  if (start == std::string_view::npos) return "";
  size_t end = term.find_last_not_of(WHITESPACE_CHARS);
  return term.substr(start, end - start + 1);
}

} // namespace howling
