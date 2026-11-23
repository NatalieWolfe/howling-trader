#include "signaling/cancellation.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "signaling/signal_handler.h"

namespace howling {

struct cancellation::callback_entry {
  std::mutex mutex;
  std::condition_variable cv;
  std::function<void()> fn;
  bool active = true;
  bool executing = false;
  std::thread::id executing_thread{};
};

namespace {

const signal_handler* resolve_signal_handler(const signal_handler* inherited) {
  if (inherited != nullptr) return inherited;
  if (signal_handler::is_initialized()) return &signal_handler::instance();
  LOG(WARNING) << "Constructed cancellation before "
               << "signal_handler::initialize(); process shutdown signals are "
               << "not registered and will not be observed.";
  return nullptr;
}

template <typename EntryPtr>
void invoke_callback_entry(EntryPtr entry) noexcept {
  if (!entry) return;
  std::function<void()> fn;
  {
    std::lock_guard lock(entry->mutex);
    if (!entry->active || !entry->fn) return;
    entry->active = false;
    entry->executing = true;
    entry->executing_thread = std::this_thread::get_id();
    fn = std::move(entry->fn);
  }
  try {
    fn();
  } catch (...) {}
  fn = nullptr;
  {
    std::lock_guard lock(entry->mutex);
    entry->executing = false;
  }
  entry->cv.notify_all();
}

} // namespace

cancellation::callback_registration::callback_registration(
    std::shared_ptr<callback_entry> entry) noexcept
    : _entry(std::move(entry)) {}

cancellation::callback_registration::callback_registration(
    callback_registration&& other) noexcept
    : _entry(std::exchange(other._entry, nullptr)) {}

cancellation::callback_registration&
cancellation::callback_registration::operator=(
    callback_registration&& other) noexcept {
  if (this != &other) {
    unregister();
    _entry = std::exchange(other._entry, nullptr);
  }
  return *this;
}

cancellation::callback_registration::~callback_registration() {
  unregister();
}

void cancellation::callback_registration::unregister() noexcept {
  auto entry = std::exchange(_entry, nullptr);
  if (!entry) return;
  std::unique_lock lock(entry->mutex);
  entry->active = false;
  entry->fn = nullptr;
  if (entry->executing &&
      entry->executing_thread != std::this_thread::get_id()) {
    entry->cv.wait(lock, [&]() { return !entry->executing; });
  }
}

cancellation::cancellation() : cancellation(nullptr) {}

cancellation::cancellation(const cancellation& parent)
    : cancellation(&parent) {}

cancellation::~cancellation() {
  _parent_registration.unregister();
}

cancellation cancellation::with_parent(const cancellation& parent) {
  return cancellation(&parent);
}

cancellation::cancellation(const cancellation* parent) : _parent(parent) {
  const signal_handler* inherited = parent ? parent->_signal_handler : nullptr;
  _signal_handler = resolve_signal_handler(inherited);
  if (_parent != nullptr) {
    _parent_registration =
        _parent->_register_child_callback([this]() { _notify_cancelled(); });
  }
}

void cancellation::cancel() noexcept {
  if (_cancelled.exchange(true, std::memory_order_acq_rel)) return;
  _notify_cancelled();
}

bool cancellation::is_cancelled() const noexcept {
  if (_cancelled.load(std::memory_order_acquire)) return true;
  if (_signal_handler && _signal_handler->has_received_signal()) return true;
  if (_parent && _parent->is_cancelled()) return true;
  return false;
}

int cancellation::received_signal() const noexcept {
  if (_signal_handler != nullptr) {
    int signal_number = _signal_handler->received_signal();
    if (signal_number != 0) return signal_number;
  }
  if (_parent) return _parent->received_signal();
  return 0;
}

bool cancellation::wait_for(
    std::chrono::steady_clock::duration duration) const {
  if (is_cancelled()) return true;
  if (duration <= std::chrono::steady_clock::duration::zero()) {
    return is_cancelled();
  }

  constexpr auto SIGNAL_POLL_INTERVAL = std::chrono::milliseconds{10};
  const auto now = std::chrono::steady_clock::now();
  const auto max_remaining = std::chrono::steady_clock::time_point::max() - now;
  const auto deadline = duration >= max_remaining
      ? std::chrono::steady_clock::time_point::max()
      : now + duration;

  std::unique_lock lock(_mutex);
  while (!is_cancelled()) {
    const auto current = std::chrono::steady_clock::now();
    if (current >= deadline) return is_cancelled();
    auto wake_time = deadline;
    if (_signal_handler != nullptr &&
        deadline - current > SIGNAL_POLL_INTERVAL) {
      wake_time = current + SIGNAL_POLL_INTERVAL;
    }
    _cv.wait_until(lock, wake_time);
  }
  return true;
}

cancellation::callback_registration
cancellation::on_cancel(std::function<void()> callback) const {
  if (!callback) return {};
  if (is_cancelled()) {
    callback();
    return {};
  }
  std::shared_ptr<callback_entry> entry;
  {
    std::lock_guard lock(_mutex);
    if (!is_cancelled()) {
      std::erase_if(
          _callbacks, [](const auto& weak) { return weak.expired(); });
      entry = std::make_shared<callback_entry>();
      entry->fn = std::move(callback);
      _callbacks.push_back(entry);
    }
  }
  if (!entry) {
    callback();
    return {};
  }
  return callback_registration(std::move(entry));
}

cancellation::callback_registration
cancellation::_register_child_callback(std::function<void()> callback) const {
  std::lock_guard lock(_mutex);
  if (_cancelled.load(std::memory_order_acquire)) return {};
  std::erase_if(_callbacks, [](const auto& weak) { return weak.expired(); });
  auto entry = std::make_shared<callback_entry>();
  entry->fn = std::move(callback);
  _callbacks.push_back(entry);
  return callback_registration(std::move(entry));
}

void cancellation::_notify_cancelled() const noexcept {
  std::vector<std::weak_ptr<callback_entry>> callbacks;
  {
    std::lock_guard lock(_mutex);
    callbacks = std::move(_callbacks);
    _callbacks.clear();
  }
  _cv.notify_all();
  for (const auto& weak_entry : callbacks) {
    invoke_callback_entry(weak_entry.lock());
  }
}

} // namespace howling
