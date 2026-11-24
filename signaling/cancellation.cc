#include "signaling/cancellation.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "signaling/signal_handler.h"

namespace howling {

struct cancellation::callback_entry {
  std::mutex mutex;
  std::condition_variable cv;
  std::function<void()> callback;
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

void invoke_callback_entry(
    const std::shared_ptr<cancellation::callback_entry>& entry) noexcept {
  if (!entry) return;
  std::function<void()> callback;
  {
    std::lock_guard lock(entry->mutex);
    if (!entry->active || !entry->callback) return;
    entry->active = false;
    entry->executing = true;
    entry->executing_thread = std::this_thread::get_id();
    callback = std::move(entry->callback);
  }
  try {
    callback();
  } catch (const std::exception& err) {
    LOG(ERROR) << "Cancellation callback threw exception: " << err.what();
  } catch (...) {
    LOG(ERROR) << "Cancellation callback threw an unknown exception.";
  }
  callback = nullptr;
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
  std::shared_ptr<callback_entry> entry = std::exchange(_entry, nullptr);
  if (!entry) return;
  std::unique_lock lock(entry->mutex);
  entry->active = false;
  entry->callback = nullptr;
  if (entry->executing &&
      entry->executing_thread != std::this_thread::get_id()) {
    entry->cv.wait(lock, [&]() { return !entry->executing; });
  }
}

cancellation::cancellation() : cancellation(nullptr) {}

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

cancellation::callback_registration
cancellation::on_cancel(std::function<void()> callback) const {
  if (!callback) {
    throw std::invalid_argument("Cancellation callback must not be empty.");
  }
  std::shared_ptr<callback_entry> entry;
  {
    std::lock_guard lock(_mutex);
    if (!is_cancelled()) {
      std::erase_if(
          _callbacks, [](const std::weak_ptr<callback_entry>& weak_entry) {
            return weak_entry.expired();
          });
      entry = std::make_shared<callback_entry>();
      entry->callback = std::move(callback);
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
  std::erase_if(
      _callbacks, [](const std::weak_ptr<callback_entry>& weak_entry) {
        return weak_entry.expired();
      });
  std::shared_ptr<callback_entry> entry = std::make_shared<callback_entry>();
  entry->callback = std::move(callback);
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
  for (const std::weak_ptr<callback_entry>& weak_entry : callbacks) {
    // std::weak_ptr::lock returns nullptr if the registration has expired,
    // which invoke_callback_entry checks for.
    invoke_callback_entry(weak_entry.lock());
  }
}

} // namespace howling
