#include "signaling/cancellation.h"

#include <atomic>

#include "absl/log/log.h"
#include "signaling/signal_handler.h"

namespace howling {
namespace {

const signal_handler* resolve_signal_handler(const signal_handler* inherited) {
  if (inherited != nullptr) return inherited;
  if (signal_handler::is_initialized()) return &signal_handler::instance();
  LOG(WARNING) << "Constructed cancellation before "
               << "signal_handler::initialize(); process shutdown signals are "
               << "not registered and will not be observed.";
  return nullptr;
}

} // namespace

cancellation::cancellation() : cancellation(nullptr) {}

cancellation cancellation::with_parent(const cancellation& parent) {
  return cancellation(&parent);
}

cancellation::cancellation(const cancellation* parent) : _parent(parent) {
  const signal_handler* inherited = parent ? parent->_signal_handler : nullptr;
  _signal_handler = resolve_signal_handler(inherited);
}

void cancellation::cancel() noexcept {
  _cancelled.store(true, std::memory_order_release);
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

} // namespace howling
