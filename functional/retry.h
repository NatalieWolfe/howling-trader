#pragma once

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace howling {

struct backoff_options {
  std::chrono::steady_clock::duration initial_delay =
      std::chrono::milliseconds{1};
  std::chrono::steady_clock::duration maximum_delay = std::chrono::seconds{1};
  std::chrono::steady_clock::duration deadline = std::chrono::seconds{60};
  double exponent = 1.2;
};

struct retriable_error : public std::runtime_error {
  explicit retriable_error(const std::string& message)
      : std::runtime_error(message) {}
  explicit retriable_error(const char* message) : std::runtime_error(message) {}
};

struct backoff_exceeded_error : public std::runtime_error {
  explicit backoff_exceeded_error(const std::string& message)
      : std::runtime_error(message) {}
  explicit backoff_exceeded_error(const char* message)
      : std::runtime_error(message) {}
};

/**
 * @brief Executes a callable with exponential backoff when `retriable_error` is
 * thrown.
 *
 * @param options
 *  Configuration for initial delay, maximum total delay, and backoff exponent.
 * @param func
 *  Callable to invoke on each attempt.
 *
 * @throws backoff_exceeded_error if `func` continues throwing `retriable_error`
 * after `options.deadline` is exceeded.
 */
template <typename Func>
decltype(auto) backoff(const backoff_options& options, Func&& func) {
  std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + options.deadline;
  std::optional<retriable_error> last_retry;
  std::chrono::steady_clock::duration next_delay = options.initial_delay;

  auto delay = [&]() -> bool {
    if (std::chrono::steady_clock::now() + next_delay >= deadline) return false;
    std::this_thread::sleep_for(next_delay);
    next_delay = std::min(
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            next_delay * options.exponent),
        options.maximum_delay);
    return true;
  };

  do {
    try {
      return func();
    } catch (const retriable_error& retry_reason) { last_retry = retry_reason; }
  } while (delay());
  throw backoff_exceeded_error(
      std::format(
          "Backoff exceeded, aborting. Last retry message: {}",
          last_retry ? last_retry->what() : "<none>"));
}

/**
 * @brief Executes a callable with default exponential backoff options.
 */
template <typename Func>
decltype(auto) backoff(Func&& func) {
  return backoff(backoff_options{}, std::forward<Func>(func));
}

} // namespace howling
