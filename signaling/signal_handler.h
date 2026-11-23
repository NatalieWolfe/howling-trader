#pragma once

namespace howling {

/**
 * @brief Process-wide singleton that captures shutdown signals.
 *
 * Installs async-signal-safe handlers for SIGINT and SIGTERM exactly once per
 * process via initialize(). The first received signal is preserved until
 * cleared with reset_signal().
 *
 * Thread safety: All methods on this class are thread-safe. Signal delivery
 * writes through a lock-free atomic and is async-signal-safe.
 */
class signal_handler {
public:
  signal_handler(const signal_handler&) = delete;
  signal_handler& operator=(const signal_handler&) = delete;
  signal_handler(signal_handler&&) = delete;
  signal_handler& operator=(signal_handler&&) = delete;

  /**
   * @brief Initializes the process-wide signal handler singleton.
   *
   * Installs async-signal-safe SIGINT and SIGTERM handlers via sigaction. This
   * function may only be called once per binary.
   *
   * @throws std::logic_error If initialize() has already been called.
   * @throws std::system_error If installing a POSIX signal handler fails.
   */
  static void initialize();

  /**
   * @brief Returns whether the signal handler singleton has been initialized.
   */
  [[nodiscard]] static bool is_initialized() noexcept;

  /**
   * @brief Returns the initialized process-wide signal handler singleton.
   *
   * @throws std::logic_error If initialize() has not been called yet.
   */
  [[nodiscard]] static signal_handler& instance();

  /**
   * @brief Returns whether a shutdown signal has been received.
   */
  [[nodiscard]] bool has_received_signal() const noexcept;

  /**
   * @brief Returns the first received shutdown signal number, or 0 if none.
   */
  [[nodiscard]] int received_signal() const noexcept;

  /**
   * @brief Clears any recorded shutdown signal.
   *
   * Intended primarily for tests or explicit recovery after handling a signal.
   */
  void reset_signal() noexcept;

private:
  signal_handler() = default;
  ~signal_handler() = default;
};

} // namespace howling
