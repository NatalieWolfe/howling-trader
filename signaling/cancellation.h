#pragma once

#include <atomic>

#include "signaling/signal_handler.h"

namespace howling {

/**
 * @brief Lightweight scoped handle for polling cooperative cancellation.
 *
 * A cancellation handle observes both local cancel() calls and, when
 * signal_handler::initialize() has already been called prior to construction,
 * process-wide shutdown signals (SIGINT and SIGTERM). It may optionally link to
 * a parent cancellation handle so cancelling a parent propagates to its
 * children while cancelling a child remains scoped to that child.
 *
 * Pass handles by const reference (const cancellation&) to consumers that
 * should only observe cancellation state.
 *
 * Thread safety: All member functions are thread-safe after construction
 * completes, provided any referenced parent cancellation outlives this
 * instance.
 */
class cancellation {
public:
  /**
   * @brief Constructs a cancellation handle, optionally linked to a parent.
   *
   * Attaches to signal_handler::instance() if it has already been initialized
   * (or inherits the parent's attachment). Otherwise logs a warning that
   * process signals are not registered and continues supporting local and
   * parent cancellation.
   *
   * @param parent Optional parent cancellation handle that, when non-null, must
   * outlive this instance.
   */
  explicit cancellation(const cancellation* parent = nullptr);

  cancellation(const cancellation&) = delete;
  cancellation& operator=(const cancellation&) = delete;
  cancellation(cancellation&&) = delete;
  cancellation& operator=(cancellation&&) = delete;

  /**
   * @brief Marks this cancellation handle as cancelled.
   */
  void cancel() noexcept;

  /**
   * @brief Returns whether cancellation has been requested from any source.
   */
  [[nodiscard]] bool is_cancelled() const noexcept;

  /**
   * @brief Returns the received shutdown signal number, or 0 if none.
   */
  [[nodiscard]] int received_signal() const noexcept;

private:
  const signal_handler* _signal_handler = nullptr;
  const cancellation* _parent = nullptr;
  std::atomic<bool> _cancelled{false};
};

} // namespace howling
