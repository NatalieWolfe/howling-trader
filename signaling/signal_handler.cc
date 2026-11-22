#include "signaling/signal_handler.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <mutex>
#include <signal.h>
#include <stdexcept>
#include <system_error>

namespace howling {
namespace {

static_assert(
    std::atomic<int>::is_always_lock_free,
    "std::atomic<int> must be lock-free for async-signal-safe handling.");

std::atomic<int> received_shutdown_signal{0};
std::mutex initialization_mutex;
std::atomic<signal_handler*> active_instance{nullptr};

} // namespace

extern "C" {

static void store_shutdown_signal(int signal_number) noexcept {
  int expected = 0;
  received_shutdown_signal.compare_exchange_strong(
      expected,
      signal_number,
      std::memory_order_release,
      std::memory_order_relaxed);
}

} // extern "C"

void signal_handler::initialize() {
  std::lock_guard<std::mutex> lock(initialization_mutex);
  if (active_instance.load(std::memory_order_acquire) != nullptr) {
    throw std::logic_error(
        "signal_handler::initialize() may only be called once per process.");
  }

  received_shutdown_signal.store(0, std::memory_order_release);

  struct sigaction action{};
  action.sa_handler = &store_shutdown_signal;
  if (sigemptyset(&action.sa_mask) != 0) {
    throw std::system_error(
        errno, std::generic_category(), "Failed to initialize signal mask");
  }
  action.sa_flags = 0;

  struct sigaction previous_sigint{};
  if (sigaction(SIGINT, &action, &previous_sigint) != 0) {
    throw std::system_error(
        errno, std::generic_category(), "Failed to install SIGINT handler");
  }
  if (sigaction(SIGTERM, &action, nullptr) != 0) {
    int saved_errno = errno;
    sigaction(SIGINT, &previous_sigint, nullptr);
    throw std::system_error(
        saved_errno,
        std::generic_category(),
        "Failed to install SIGTERM handler");
  }

  static signal_handler singleton;
  active_instance.store(&singleton, std::memory_order_release);
}

bool signal_handler::is_initialized() noexcept {
  return active_instance.load(std::memory_order_acquire) != nullptr;
}

signal_handler& signal_handler::instance() {
  signal_handler* handler = active_instance.load(std::memory_order_acquire);
  if (handler == nullptr) {
    throw std::logic_error(
        "signal_handler::instance() called before initialize().");
  }
  return *handler;
}

bool signal_handler::is_signal_received() const noexcept {
  return received_signal() != 0;
}

int signal_handler::received_signal() const noexcept {
  return received_shutdown_signal.load(std::memory_order_acquire);
}

void signal_handler::reset_signal() noexcept {
  received_shutdown_signal.store(0, std::memory_order_release);
}

} // namespace howling
