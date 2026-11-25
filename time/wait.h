#pragma once

#include <chrono>

#include "signaling/cancellation.h"

namespace howling {

enum class wait_result { COMPLETED, CANCELLED };

/**
 * @brief Blocks until duration elapses or cancellation occurs.
 *
 * @param duration
 *  Maximum duration to wait.
 * @param cancel
 *  Cooperative cancellation handle that interrupts the wait early.
 * @return wait_result::CANCELLED if cancelled, or wait_result::COMPLETED if the
 * timeout elapsed without cancellation.
 */
[[nodiscard]] wait_result wait_for(
    std::chrono::steady_clock::duration duration, const cancellation& cancel);

template <typename Rep, typename Period>
[[nodiscard]] wait_result wait_for(
    std::chrono::duration<Rep, Period> duration, const cancellation& cancel) {
  return wait_for(
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration),
      cancel);
}

} // namespace howling
