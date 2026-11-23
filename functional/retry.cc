#include "functional/retry.h"

#include <chrono>

namespace howling::retry_internal {

std::chrono::steady_clock::time_point
compute_deadline(std::chrono::steady_clock::duration duration) {
  const std::chrono::steady_clock::time_point now =
      std::chrono::steady_clock::now();
  if (duration >= std::chrono::steady_clock::time_point::max() - now) {
    return std::chrono::steady_clock::time_point::max();
  }
  return now + duration;
}

} // namespace howling::retry_internal
