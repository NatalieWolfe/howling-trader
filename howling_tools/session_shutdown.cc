#include "howling_tools/session_shutdown.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <signal.h>

namespace howling {
namespace {

using ::std::chrono::choose;
using ::std::chrono::days;
using ::std::chrono::floor;
using ::std::chrono::hours;
using ::std::chrono::local_days;
using ::std::chrono::locate_zone;
using ::std::chrono::minutes;
using ::std::chrono::system_clock;
using ::std::chrono::time_zone;
using ::std::chrono::zoned_time;

std::atomic<int> received_signal{0};
static_assert(
    std::atomic<int>::is_always_lock_free,
    "Shutdown signal flag must be lock-free for async-signal-safety.");

system_clock::duration
effective_poll_interval(system_clock::duration configured_interval) {
  if (configured_interval > system_clock::duration::zero()) {
    return configured_interval;
  }
  return DEFAULT_SHUTDOWN_POLL_INTERVAL;
}

} // namespace

extern "C" {

static void handle_shutdown_signal(int signal_number) noexcept {
  received_signal.store(signal_number, std::memory_order_relaxed);
}

} // extern "C"

int received_shutdown_signal() noexcept {
  return received_signal.load(std::memory_order_relaxed);
}

bool shutdown_signal_received() noexcept {
  return received_shutdown_signal() != 0;
}

void reset_shutdown_signal() noexcept {
  received_signal.store(0, std::memory_order_relaxed);
}

scoped_signal_handler::scoped_signal_handler() {
  reset_shutdown_signal();

  struct sigaction action{};
  action.sa_handler = handle_shutdown_signal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (::sigaction(SIGINT, &action, &_previous_sigint) == 0) {
    _installed_sigint = true;
  }
  if (::sigaction(SIGTERM, &action, &_previous_sigterm) == 0) {
    _installed_sigterm = true;
  }
}

scoped_signal_handler::~scoped_signal_handler() {
  if (_installed_sigint) ::sigaction(SIGINT, &_previous_sigint, nullptr);
  if (_installed_sigterm) ::sigaction(SIGTERM, &_previous_sigterm, nullptr);
}

system_clock::time_point market_shutdown_deadline(
    system_clock::time_point session_start,
    hours shutdown_hour,
    minutes shutdown_minute) {
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
    system_clock::duration max_lag,
    const wait_options& options) {
  const system_clock::duration poll_interval =
      effective_poll_interval(options.poll_interval);
  while (true) {
    if (options.stop_requested()) return shutdown_reason::STOP_REQUESTED;
    const system_clock::time_point current_time = options.now();
    if (current_time >= shutdown_deadline) {
      return shutdown_reason::DEADLINE_REACHED;
    }
    if (current_time - read_state_time() <= max_lag) {
      return shutdown_reason::CAUGHT_UP;
    }
    const system_clock::duration remaining = shutdown_deadline - current_time;
    options.sleep(std::min(poll_interval, remaining));
  }
}

shutdown_reason wait_for_shutdown(
    system_clock::time_point shutdown_deadline, const wait_options& options) {
  const system_clock::duration poll_interval =
      effective_poll_interval(options.poll_interval);
  while (true) {
    if (options.stop_requested()) return shutdown_reason::STOP_REQUESTED;
    const system_clock::time_point current_time = options.now();
    if (current_time >= shutdown_deadline) {
      return shutdown_reason::DEADLINE_REACHED;
    }
    const system_clock::duration remaining = shutdown_deadline - current_time;
    options.sleep(std::min(poll_interval, remaining));
  }
}

} // namespace howling
