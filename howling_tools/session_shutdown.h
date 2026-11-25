#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <thread>

#include "signaling/cancellation.h"

namespace howling {

/**
 * @brief Reason why a session wait loop terminated for shutdown.
 */
enum class shutdown_reason {
  // The wall-clock session shutdown deadline was reached.
  DEADLINE_REACHED,
  // A cancellation request or shutdown signal interrupted the wait.
  STOP_REQUESTED,
};

/**
 * @brief Injectable hooks and polling configuration for session wait loops.
 *
 * Thread-compatible: configure before passing to a wait function.
 */
struct wait_options {
  // Returns the current wall-clock time. Must not be null.
  std::function<std::chrono::system_clock::time_point()> now = []() {
    return std::chrono::system_clock::now();
  };
  // Sleeps for the requested short slice between wall-clock checks. Must not be
  // null.
  std::function<void(std::chrono::system_clock::duration)> sleep =
      [](std::chrono::system_clock::duration duration) {
        std::this_thread::sleep_for(duration);
      };
  // Maximum sleep slice between wall-clock and cancellation checks. When unset,
  // defaults to 1 second. Must be strictly positive when set.
  std::optional<std::chrono::system_clock::duration> poll_interval =
      std::nullopt;
};

/**
 * @brief Computes the default 15:45 ET wall-clock shutdown deadline for the
 * trading session containing `session_start`.
 *
 * Resolves the calendar date of `session_start` in the `America/New_York`
 * timezone and returns the UTC `system_clock::time_point` corresponding to
 * `15:45:00` on that same Eastern date.
 *
 * Computing this absolute deadline once at session startup ensures that waking
 * from system suspend after the deadline—including past midnight ET on the
 * following calendar day—immediately satisfies `now >= deadline`, and starting
 * after `15:45 ET` exits promptly instead of sleeping until the next day.
 *
 * @param session_start Wall-clock time when the trading session started.
 *
 * @return Absolute UTC deadline for shutting down the session.
 */
[[nodiscard]] std::chrono::system_clock::time_point
market_shutdown_deadline(std::chrono::system_clock::time_point session_start);

/**
 * @brief Computes a custom wall-clock shutdown deadline for the trading session
 * containing `session_start`.
 *
 * Resolves the calendar date of `session_start` in the `America/New_York`
 * timezone and returns the UTC `system_clock::time_point` corresponding to
 * `shutdown_hour:shutdown_minute:00` on that same Eastern date.
 *
 * @param session_start Wall-clock time when the trading session started.
 * @param shutdown_hour Market-local hour for shutdown, in `[0, 23]`.
 * @param shutdown_minute Market-local minute for shutdown, in `[0, 59]`.
 *
 * @throws std::invalid_argument if `shutdown_hour` is outside `[0, 23]` or
 * `shutdown_minute` is outside `[0, 59]`.
 *
 * @return Absolute UTC deadline for shutting down the session.
 */
[[nodiscard]] std::chrono::system_clock::time_point market_shutdown_deadline(
    std::chrono::system_clock::time_point session_start,
    std::chrono::hours shutdown_hour,
    std::chrono::minutes shutdown_minute);

/**
 * @brief Waits until the trading state catches up to wall-clock time, the
 * session shutdown deadline is reached, or cancellation is requested.
 *
 * @param read_state_time
 *  Callback returning the latest processed data time. Must not be null.
 * @param shutdown_deadline
 *  Absolute wall-clock deadline for session exit.
 * @param cancel
 *  Cancellation handle polled for shutdown signals or local cancellation.
 * @param max_lag
 *  Maximum acceptable lag behind wall-clock time. When unset, defaults to 2
 *  minutes. Must not be negative when set.
 * @param options
 *  Injectable clock, sleep hook, and optional poll interval.
 *
 * @throws std::invalid_argument if `read_state_time` is null, `max_lag` is
 * negative, `options.now` or `options.sleep` is null, or
 * `options.poll_interval` is non-positive.
 *
 * @return `std::nullopt` when caught up, or the `shutdown_reason` that
 * terminated the wait early.
 */
[[nodiscard]] std::optional<shutdown_reason> wait_for_catch_up(
    const std::function<std::chrono::system_clock::time_point()>&
        read_state_time,
    std::chrono::system_clock::time_point shutdown_deadline,
    const cancellation& cancel,
    std::optional<std::chrono::system_clock::duration> max_lag = std::nullopt,
    const wait_options& options = {});

/**
 * @brief Polls wall-clock time in short intervals until `shutdown_deadline` is
 * reached or cancellation is requested.
 *
 * @param shutdown_deadline
 *  Absolute wall-clock deadline for session exit.
 * @param cancel
 *  Cancellation handle polled for shutdown signals or local cancellation.
 * @param options
 *  Injectable clock, sleep hook, and optional poll interval.
 *
 * @throws std::invalid_argument if `options.now` or `options.sleep` is null, or
 * `options.poll_interval` is non-positive.
 *
 * @return `shutdown_reason::DEADLINE_REACHED` or
 * `shutdown_reason::STOP_REQUESTED`.
 */
[[nodiscard]] shutdown_reason wait_for_shutdown(
    std::chrono::system_clock::time_point shutdown_deadline,
    const cancellation& cancel,
    const wait_options& options = {});

} // namespace howling
