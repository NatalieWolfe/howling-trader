#pragma once

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "signaling/cancellation.h"

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

struct backoff_cancelled_error : public std::runtime_error {
  explicit backoff_cancelled_error(const std::string& message)
      : std::runtime_error(message) {}
  explicit backoff_cancelled_error(const char* message)
      : std::runtime_error(message) {}
};

namespace retry_internal {

inline std::chrono::steady_clock::time_point
compute_deadline(std::chrono::steady_clock::duration duration) {
  const auto now = std::chrono::steady_clock::now();
  if (duration >= std::chrono::steady_clock::time_point::max() - now) {
    return std::chrono::steady_clock::time_point::max();
  }
  return now + duration;
}

template <typename Func>
decltype(auto) backoff_impl(
    const backoff_options& options, const cancellation* cancel, Func&& func) {
  const std::chrono::steady_clock::time_point deadline =
      compute_deadline(options.deadline);
  std::optional<retriable_error> last_retry;
  std::chrono::steady_clock::duration next_delay = options.initial_delay;

  auto delay = [&]() -> bool {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline || next_delay >= deadline - now) return false;
    if (cancel != nullptr) {
      if (cancel->wait_for(next_delay)) {
        throw backoff_cancelled_error("Backoff cancelled.");
      }
    } else {
      std::this_thread::sleep_for(next_delay);
    }
    next_delay = std::min(
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            next_delay * options.exponent),
        options.maximum_delay);
    return true;
  };

  do {
    if (cancel != nullptr && cancel->is_cancelled()) {
      throw backoff_cancelled_error("Backoff cancelled.");
    }
    try {
      return func();
    } catch (const retriable_error& retry_reason) {
      if (cancel != nullptr && cancel->is_cancelled()) {
        throw backoff_cancelled_error("Backoff cancelled.");
      }
      last_retry = retry_reason;
    }
  } while (delay());
  throw backoff_exceeded_error(
      std::format(
          "Backoff exceeded, aborting. Last retry message: {}",
          last_retry ? last_retry->what() : "<none>"));
}

} // namespace retry_internal

/**
 * @brief Executes a callable with exponential backoff and cooperative
 * cancellation when `retriable_error` is thrown.
 *
 * @param options
 *  Configuration for initial delay, maximum total delay, and backoff exponent.
 * @param cancel
 *  Cooperative cancellation handle that interrupts backoff sleeps immediately.
 * @param func
 *  Callable to invoke on each attempt.
 *
 * @throws backoff_cancelled_error if `cancel` is triggered before or during
 * backoff.
 * @throws backoff_exceeded_error if `func` continues throwing `retriable_error`
 * after `options.deadline` is exceeded.
 */
template <typename Func>
decltype(auto) backoff(
    const backoff_options& options, const cancellation& cancel, Func&& func) {
  return retry_internal::backoff_impl(
      options, &cancel, std::forward<Func>(func));
}

/**
 * @brief Executes a callable with default exponential backoff options and
 * cooperative cancellation.
 */
template <typename Func>
decltype(auto) backoff(const cancellation& cancel, Func&& func) {
  return backoff(backoff_options{}, cancel, std::forward<Func>(func));
}

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
  return retry_internal::backoff_impl(
      options, nullptr, std::forward<Func>(func));
}

/**
 * @brief Executes a callable with default exponential backoff options.
 */
template <typename Func>
decltype(auto) backoff(Func&& func) {
  return backoff(backoff_options{}, std::forward<Func>(func));
}

} // namespace howling
