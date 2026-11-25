#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "signaling/signal_handler.h"

namespace howling {

/**
 * @brief Scoped handle for cooperative cancellation polling and callbacks.
 *
 * A cancellation handle observes both local cancel() calls and, when
 * signal_handler::initialize() has already been called prior to construction,
 * process-wide shutdown signals (SIGINT and SIGTERM). It may optionally link to
 * a parent cancellation handle so cancelling a parent propagates immediately to
 * its children while cancelling a child remains scoped to that child.
 *
 * Pass handles by const reference (const cancellation&) to consumers that
 * should only observe cancellation state or register cancellation callbacks.
 *
 * Thread safety: All member functions are thread-safe after construction
 * completes, provided any referenced parent cancellation outlives this
 * instance.
 */
class cancellation {
public:
  struct callback_entry;

  /**
   * @brief Scoped RAII handle for a registered cancellation callback.
   *
   * Automatically unregisters the callback on destruction if it has not been
   * invoked yet. If the callback is currently executing on another thread,
   * unregister() and destruction wait for execution to finish.
   */
  class callback_registration {
  public:
    callback_registration() = default;
    callback_registration(const callback_registration&) = delete;
    callback_registration& operator=(const callback_registration&) = delete;
    callback_registration(callback_registration&& other) noexcept;
    callback_registration& operator=(callback_registration&& other) noexcept;
    ~callback_registration();

    /**
     * @brief Unregisters the callback if it has not yet been invoked.
     */
    void unregister() noexcept;

  private:
    friend class cancellation;
    explicit callback_registration(
        std::shared_ptr<callback_entry> entry) noexcept;

    std::shared_ptr<callback_entry> _entry;
  };

  /**
   * @brief Constructs a root cancellation handle.
   *
   * Attaches to signal_handler::instance() if it has already been initialized.
   * Otherwise logs a warning that process signals are not registered and
   * continues supporting local cancellation.
   */
  cancellation();

  /**
   * @brief Constructs a cancellation handle linked to a parent.
   *
   * Inherits the parent's signal handler attachment, or attaches to
   * signal_handler::instance() if it has already been initialized (otherwise
   * logs a warning that process signals are not registered).
   *
   * @param parent
   *  Parent cancellation handle that must outlive the returned child instance.
   */
  [[nodiscard]] static cancellation with_parent(const cancellation& parent);
  static cancellation with_parent(const cancellation&&) = delete;

  cancellation(const cancellation&) = delete;
  cancellation& operator=(const cancellation&) = delete;
  cancellation(cancellation&&) = delete;
  cancellation& operator=(cancellation&&) = delete;

  ~cancellation();

  /**
   * @brief Marks this cancellation handle as cancelled and notifies registered
   * callbacks.
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

  /**
   * @brief Registers a callback to be invoked immediately upon cancellation.
   *
   * If is_cancelled() is already true, invokes callback synchronously and
   * returns an empty registration.
   *
   * @throws std::invalid_argument if callback is empty.
   */
  [[nodiscard]] callback_registration
  on_cancel(std::function<void()> callback) const;

private:
  explicit cancellation(const cancellation* parent);

  [[nodiscard]] callback_registration
  _register_child_callback(std::function<void()> callback) const;
  void _notify_cancelled() const noexcept;

  const signal_handler* _signal_handler = nullptr;
  const cancellation* _parent = nullptr;
  std::atomic<bool> _cancelled{false};
  mutable std::mutex _mutex;
  mutable std::vector<std::weak_ptr<callback_entry>> _callbacks;
  callback_registration _parent_registration;
};

} // namespace howling
