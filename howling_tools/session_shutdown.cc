#include "howling_tools/session_shutdown.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "signaling/cancellation.h"

namespace howling {
namespace {

using ::std::chrono::choose;
using ::std::chrono::days;
using ::std::chrono::floor;
using ::std::chrono::hours;
using ::std::chrono::local_days;
using ::std::chrono::locate_zone;
using ::std::chrono::minutes;
using ::std::chrono::seconds;
using ::std::chrono::system_clock;
using ::std::chrono::time_zone;
using ::std::chrono::zoned_time;

constexpr std::string_view MARKET_TIME_ZONE_NAME = "America/New_York";
constexpr hours DEFAULT_SHUTDOWN_HOUR{15};
constexpr minutes DEFAULT_SHUTDOWN_MINUTE{45};
constexpr seconds DEFAULT_SHUTDOWN_POLL_INTERVAL{1};
constexpr minutes DEFAULT_CATCH_UP_LAG_THRESHOLD{2};

system_clock::duration resolve_poll_interval(const wait_options& options) {
  if (!options.now) {
    throw std::invalid_argument("wait_options.now must not be null.");
  }
  if (!options.sleep) {
    throw std::invalid_argument("wait_options.sleep must not be null.");
  }
  if (!options.poll_interval.has_value()) {
    return DEFAULT_SHUTDOWN_POLL_INTERVAL;
  }
  if (*options.poll_interval <= system_clock::duration::zero()) {
    throw std::invalid_argument("wait_options.poll_interval must be positive.");
  }
  return *options.poll_interval;
}

system_clock::duration
resolve_max_lag(std::optional<system_clock::duration> max_lag) {
  if (!max_lag.has_value()) { return DEFAULT_CATCH_UP_LAG_THRESHOLD; }
  if (*max_lag < system_clock::duration::zero()) {
    throw std::invalid_argument("max_lag must not be negative.");
  }
  return *max_lag;
}

} // namespace

system_clock::time_point
market_shutdown_deadline(system_clock::time_point session_start) {
  return market_shutdown_deadline(
      session_start, DEFAULT_SHUTDOWN_HOUR, DEFAULT_SHUTDOWN_MINUTE);
}

system_clock::time_point market_shutdown_deadline(
    system_clock::time_point session_start,
    hours shutdown_hour,
    minutes shutdown_minute) {
  if (shutdown_hour < hours{0} || shutdown_hour >= hours{24}) {
    throw std::invalid_argument("shutdown_hour must be in [0, 23].");
  }
  if (shutdown_minute < minutes{0} || shutdown_minute >= minutes{60}) {
    throw std::invalid_argument("shutdown_minute must be in [0, 59].");
  }
  static const time_zone* const MARKET_TIME_ZONE =
      locate_zone(MARKET_TIME_ZONE_NAME);
  const zoned_time local_start{MARKET_TIME_ZONE, session_start};
  const local_days session_day = floor<days>(local_start.get_local_time());
  const auto local_deadline = session_day + shutdown_hour + shutdown_minute;
  const zoned_time deadline{MARKET_TIME_ZONE, local_deadline, choose::earliest};
  return deadline.get_sys_time();
}

shutdown_reason wait_for_catch_up(
    const std::function<system_clock::time_point()>& read_state_time,
    system_clock::time_point shutdown_deadline,
    const cancellation& cancel,
    std::optional<system_clock::duration> max_lag,
    const wait_options& options) {
  if (!read_state_time) {
    throw std::invalid_argument("read_state_time must not be null.");
  }
  const system_clock::duration effective_max_lag = resolve_max_lag(max_lag);
  const system_clock::duration poll_interval = resolve_poll_interval(options);
  while (true) {
    if (cancel.is_cancelled()) return shutdown_reason::STOP_REQUESTED;
    const system_clock::time_point current_time = options.now();
    if (current_time >= shutdown_deadline) {
      return shutdown_reason::DEADLINE_REACHED;
    }
    if (current_time - read_state_time() <= effective_max_lag) {
      return shutdown_reason::CAUGHT_UP;
    }
    const system_clock::duration remaining = shutdown_deadline - current_time;
    options.sleep(std::min(poll_interval, remaining));
  }
}

shutdown_reason wait_for_shutdown(
    system_clock::time_point shutdown_deadline,
    const cancellation& cancel,
    const wait_options& options) {
  const system_clock::duration poll_interval = resolve_poll_interval(options);
  while (true) {
    if (cancel.is_cancelled()) return shutdown_reason::STOP_REQUESTED;
    const system_clock::time_point current_time = options.now();
    if (current_time >= shutdown_deadline) {
      return shutdown_reason::DEADLINE_REACHED;
    }
    const system_clock::duration remaining = shutdown_deadline - current_time;
    options.sleep(std::min(poll_interval, remaining));
  }
}

} // namespace howling
