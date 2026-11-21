#pragma once

#include <chrono>
#include <functional>
#include <signal.h>
#include <string_view>
#include <thread>

namespace howling {

inline constexpr std::string_view MARKET_TIME_ZONE_NAME = "America/New_York";
inline constexpr std::chrono::hours DEFAULT_SHUTDOWN_HOUR{15};
inline constexpr std::chrono::minutes DEFAULT_SHUTDOWN_MINUTE{45};
inline constexpr std::chrono::seconds DEFAULT_SHUTDOWN_POLL_INTERVAL{1};
inline constexpr std::chrono::minutes DEFAULT_CATCH_UP_LAG_THRESHOLD{2};

/**
 * @brief Reason why a session wait loop terminated.
 */
enum class shutdown_reason {
  // The trading state caught up with the current wall-clock time.
  CAUGHT_UP,
  // The wall-clock session shutdown deadline was reached.
  DEADLINE_REACHED,
  // A stop signal or external stop request interrupted the wait.
  STOP_REQUESTED,
};

/**
 * @brief Returns the signal number recorded by the shutdown signal handler, or
 * zero if no shutdown signal has been received.
 *
 * Thread-safe and async-signal-safe.
 */
[[nodiscard]] int received_shutdown_signal() noexcept;

/**
 * @brief Returns true if a shutdown signal (`SIGINT` or `SIGTERM`) has been
 * received.
 *
 * Thread-safe and async-signal-safe.
 */
[[nodiscard]] bool shutdown_signal_received() noexcept;

/**
 * @brief Resets the recorded shutdown signal back to zero.
 *
 * Thread-safe and async-signal-safe.
 */
void reset_shutdown_signal() noexcept;

/**
 * @brief RAII guard that installs async-signal-safe `SIGINT` and `SIGTERM`
 * handlers and restores the previous handlers upon destruction.
 *
 * Thread-compatible: construct and destroy on the main thread around the
 * session lifecycle.
 */
class scoped_signal_handler {
public:
  scoped_signal_handler();
  ~scoped_signal_handler();

  scoped_signal_handler(const scoped_signal_handler&) = delete;
  scoped_signal_handler& operator=(const scoped_signal_handler&) = delete;
  scoped_signal_handler(scoped_signal_handler&&) = delete;
  scoped_signal_handler& operator=(scoped_signal_handler&&) = delete;

private:
  struct sigaction _previous_sigint{};
  struct sigaction _previous_sigterm{};
  bool _installed_sigint = false;
  bool _installed_sigterm = false;
};

/**
 * @brief Injectable hooks and polling configuration for session wait loops.
 *
 * Thread-compatible: configure before passing to a wait function.
 */
struct wait_options {
  // Returns the current wall-clock time.
  std::function<std::chrono::system_clock::time_point()> now = []() {
    return std::chrono::system_clock::now();
  };
  // Returns true when an external stop (for example, SIGINT/SIGTERM) is
  // requested.
  std::function<bool()> stop_requested = []() {
    return shutdown_signal_received();
  };
  // Sleeps for the requested short slice between wall-clock checks.
  std::function<void(std::chrono::system_clock::duration)> sleep =
      [](std::chrono::system_clock::duration duration) {
        std::this_thread::sleep_for(duration);
      };
  // Maximum sleep slice between wall-clock and stop-signal checks.
  std::chrono::system_clock::duration poll_interval =
      DEFAULT_SHUTDOWN_POLL_INTERVAL;
};

/**
 * @brief Computes the wall-clock shutdown deadline for the trading session
 * containing `session_start`.
 *
 * Resolves the calendar date of `session_start` in the `America/New_York`
 * timezone and returns the UTC `system_clock::time_point` corresponding to
 * `shutdown_hour:shutdown_minute:00` on that same Eastern date.
 *
 * Computing this absolute deadline once at session startup ensures that waking
 * from system suspend after the deadline—including past midnight ET on the
 * following calendar day—immediately satisfies `now >= deadline`, and starting
 * after `15:45 ET` exits promptly instead of sleeping until the next day.
 *
 * @param session_start Wall-clock time when the trading session started.
 * @param shutdown_hour Market-local hour for shutdown (default 15).
 * @param shutdown_minute Market-local minute for shutdown (default 45).
 * @return Absolute UTC deadline for shutting down the session.
 */
[[nodiscard]] std::chrono::system_clock::time_point market_shutdown_deadline(
    std::chrono::system_clock::time_point session_start,
    std::chrono::hours shutdown_hour = DEFAULT_SHUTDOWN_HOUR,
    std::chrono::minutes shutdown_minute = DEFAULT_SHUTDOWN_MINUTE);

/**
 * @brief Waits until the trading state catches up to wall-clock time, the
 * session shutdown deadline is reached, or a stop is requested.
 *
 * @param read_state_time Callback returning the latest processed data time.
 * @param shutdown_deadline Absolute wall-clock deadline for session exit.
 * @param max_lag Maximum acceptable lag behind wall-clock time.
 * @param options Injectable clock, stop predicate, sleep hook, and poll
 *     interval.
 * @return The reason the catch-up wait loop terminated.
 */
[[nodiscard]] shutdown_reason wait_for_catch_up(
    const std::function<std::chrono::system_clock::time_point()>&
        read_state_time,
    std::chrono::system_clock::time_point shutdown_deadline,
    std::chrono::system_clock::duration max_lag =
        DEFAULT_CATCH_UP_LAG_THRESHOLD,
    const wait_options& options = {});

/**
 * @brief Polls wall-clock time in short intervals until `shutdown_deadline` is
 * reached or a stop is requested.
 *
 * @param shutdown_deadline Absolute wall-clock deadline for session exit.
 * @param options Injectable clock, stop predicate, sleep hook, and poll
 *     interval.
 * @return `shutdown_reason::DEADLINE_REACHED` or
 *     `shutdown_reason::STOP_REQUESTED`.
 */
[[nodiscard]] shutdown_reason wait_for_shutdown(
    std::chrono::system_clock::time_point shutdown_deadline,
    const wait_options& options = {});

} // namespace howling
