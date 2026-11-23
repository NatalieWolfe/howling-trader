#include "time/wait.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "signaling/cancellation.h"

namespace howling {

wait_result wait_for(
    std::chrono::steady_clock::duration duration, const cancellation& cancel) {
  if (cancel.is_cancelled()) return wait_result::CANCELLED;
  if (duration <= std::chrono::steady_clock::duration::zero()) {
    return cancel.is_cancelled() ? wait_result::CANCELLED
                                 : wait_result::COMPLETED;
  }

  const std::chrono::steady_clock::time_point now =
      std::chrono::steady_clock::now();
  const std::chrono::steady_clock::duration max_remaining =
      std::chrono::steady_clock::time_point::max() - now;
  const std::chrono::steady_clock::time_point deadline =
      duration >= max_remaining ? std::chrono::steady_clock::time_point::max()
                                : now + duration;

  std::mutex mutex;
  std::condition_variable cv;
  cancellation::callback_registration registration = cancel.on_cancel([&]() {
    std::lock_guard lock(mutex);
    cv.notify_all();
  });

  std::unique_lock lock(mutex);
  cv.wait_until(lock, deadline, [&]() { return cancel.is_cancelled(); });
  return cancel.is_cancelled() ? wait_result::CANCELLED
                               : wait_result::COMPLETED;
}

} // namespace howling
