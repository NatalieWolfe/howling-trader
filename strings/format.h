#pragma once

#include <cctype>
#include <chrono>
#include <string>
#include <string_view>

namespace howling {

inline char to_upper(char c) {
  return std::toupper(c);
}

std::string to_string(std::chrono::system_clock::time_point time);

// Escapes reserved characters for Telegram MarkdownV2 format.
std::string escape_markdown_v2(std::string_view text);

} // namespace howling
