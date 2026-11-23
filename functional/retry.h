#pragma once

#include <algorithm>
#include <chrono>
#include <format>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

struct poll_options {
  std::chrono::system_clock::duration interval = std::chrono::milliseconds{100};
  std::optional<std::reference_wrapper<const cancellation>> cancel =
      std::nullopt;
  std::optional<std::chrono::system_clock::time_point> deadline = std::nullopt;
  std::function<std::chrono::system_clock::time_point()> now = []() {
    return std::chrono::system_clock::now();
  };
  std::function<void(std::chrono::system_clock::duration)> sleep = nullptr;
};

struct retriable_error : public std::runtime_error {
  retriable_error() : std::runtime_error("") {}
  explicit retriable_error(const std::string& message)
      : std::runtime_error(message) {}
  explicit retriable_error(const char* message) : std::runtime_error(message) {}
};

struct retry_exceeded_error : public std::runtime_error {
  explicit retry_exceeded_error(const std::string& message)
      : std::runtime_error(message) {}
  explicit retry_exceeded_error(const char* message)
      : std::runtime_error(message) {}
};

struct retry_cancelled_error : public std::runtime_error {
  explicit retry_cancelled_error(const std::string& message)
      : std::runtime_error(message) {}
  explicit retry_cancelled_error(const char* message)
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

inline std::string_view
format_last_retry_message(const std::optional<retriable_error>& last_retry) {
  if (!last_retry.has_value()) return "<none>";
  const std::string_view message{last_retry->what()};
  return message.empty() ? "<none>" : message;
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
        throw retry_cancelled_error("Backoff cancelled.");
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
      throw retry_cancelled_error("Backoff cancelled.");
    }
    try {
      return func();
    } catch (const retriable_error& retry_reason) {
      if (cancel != nullptr && cancel->is_cancelled()) {
        throw retry_cancelled_error("Backoff cancelled.");
      }
      last_retry = retry_reason;
    }
  } while (delay());
  throw retry_exceeded_error(
      std::format(
          "Backoff exceeded, aborting. Last retry message: {}",
          format_last_retry_message(last_retry)));
}

template <typename Func>
decltype(auto) poll_impl(const poll_options& options, Func&& func) {
  if (options.interval <= std::chrono::system_clock::duration::zero()) {
    throw std::invalid_argument("poll_options.interval must be positive.");
  }
  if (!options.now) {
    throw std::invalid_argument("poll_options.now must not be null.");
  }

  const cancellation* cancel =
      options.cancel.has_value() ? &options.cancel->get() : nullptr;
  std::optional<retriable_error> last_retry;

  auto throw_exceeded = [&]() {
    throw retry_exceeded_error(
        std::format(
            "Poll deadline exceeded, aborting. Last retry message: {}",
            format_last_retry_message(last_retry)));
  };

  while (true) {
    if (cancel != nullptr && cancel->is_cancelled()) {
      throw retry_cancelled_error("Poll cancelled.");
    }
    const std::chrono::system_clock::time_point current_time = options.now();
    if (options.deadline.has_value() && current_time >= *options.deadline) {
      throw_exceeded();
    }
    try {
      return func();
    } catch (const retriable_error& retry_reason) {
      if (cancel != nullptr && cancel->is_cancelled()) {
        throw retry_cancelled_error("Poll cancelled.");
      }
      last_retry = retry_reason;
    }

    std::chrono::system_clock::duration sleep_duration = options.interval;
    if (options.deadline.has_value()) {
      const std::chrono::system_clock::time_point now_after_attempt =
          options.now();
      if (now_after_attempt >= *options.deadline) throw_exceeded();
      sleep_duration =
          std::min(sleep_duration, *options.deadline - now_after_attempt);
    }

    if (options.sleep) {
      options.sleep(sleep_duration);
    } else if (cancel != nullptr) {
      if (cancel->wait_for(sleep_duration)) {
        throw retry_cancelled_error("Poll cancelled.");
      }
    } else {
      std::this_thread::sleep_for(sleep_duration);
    }
  }
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
 * @throws retry_cancelled_error if `cancel` is triggered before or during
 * backoff.
 * @throws retry_exceeded_error if `func` continues throwing `retriable_error`
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
 * @throws retry_exceeded_error if `func` continues throwing `retriable_error`
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

/**
 * @brief Polls a callable at a fixed interval while `retriable_error` is
 * thrown, up to an optional wall-clock deadline or cancellation.
 *
 * @param options
 *  Polling interval, optional cancellation handle, optional wall-clock
 *  deadline, and optional clock/sleep hooks.
 * @param func
 *  Callable invoked on each poll iteration. Throw `retriable_error` to
 *  continue polling.
 *
 * @throws std::invalid_argument if `options.interval` is non-positive or
 * `options.now` is null.
 * @throws retry_cancelled_error if `options.cancel` is triggered before or
 * during polling.
 * @throws retry_exceeded_error if `options.deadline` is reached before `func`
 * succeeds.
 */
template <typename Func>
decltype(auto) poll(const poll_options& options, Func&& func) {
  return retry_internal::poll_impl(options, std::forward<Func>(func));
}

/**
 * @brief Polls at a fixed interval until the wall-clock deadline is reached or
 * cancellation is requested.
 *
 * @param options
 *  Polling interval, optional cancellation handle, optional wall-clock
 *  deadline, and optional clock/sleep hooks.
 *
 * @throws std::invalid_argument if `options.interval` is non-positive or
 * `options.now` is null.
 * @throws retry_cancelled_error if `options.cancel` is triggered before or
 * during polling.
 * @throws retry_exceeded_error when `options.deadline` is reached.
 */
inline void poll(const poll_options& options) {
  poll(options, []() { throw retriable_error{}; });
}

} // namespace howling
