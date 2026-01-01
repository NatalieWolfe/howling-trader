#include "strings/format.h"

#include <chrono>
#include <format>
#include <string>

namespace howling {

std::string to_string(std::chrono::system_clock::time_point time) {
  return std::format("{:%F %T}", time);
}

} // namespace howling
