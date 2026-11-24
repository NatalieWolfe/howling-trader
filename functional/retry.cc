#include "functional/retry.h"

#include <chrono>
#include <optional>
#include <string_view>

namespace howling {
namespace retry_internal {

std::chrono::steady_clock::time_point
compute_deadline(std::chrono::steady_clock::duration duration) {
  const std::chrono::steady_clock::time_point now =
      std::chrono::steady_clock::now();
  if (duration >= std::chrono::steady_clock::time_point::max() - now) {
    return std::chrono::steady_clock::time_point::max();
  }
  return now + duration;
}

std::string_view
format_last_retry_message(const std::optional<retriable_error>& last_retry) {
  if (!last_retry) return "<none>";
  const std::string_view message{last_retry->what()};
  return message.empty() ? "<none>" : message;
}

} // namespace retry_internal

void poll(const poll_options& options) {
  poll(options, []() { throw retriable_error{}; });
}

} // namespace howling
