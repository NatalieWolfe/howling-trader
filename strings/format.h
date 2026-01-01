#pragma once

#include <cctype>
#include <chrono>
#include <string>

namespace howling {

inline char to_upper(char c) {
  return std::toupper(c);
}

std::string to_string(std::chrono::system_clock::time_point time);

} // namespace howling
