#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace howling::storage {

struct auth_token {
  using time_point = std::chrono::system_clock::time_point;

  std::string service_name;
  std::string refresh_token;
  std::optional<std::string> notice_token;
  std::optional<time_point> last_notified_at;
  std::optional<time_point> expires_at;
  time_point updated_at;
};

} // namespace howling::storage
