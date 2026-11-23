#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"

namespace howling::testing {

/**
 * @brief Scoped Abseil log sink that captures log messages for tests.
 *
 * Automatically registers itself with Abseil's global log sink registry on
 * construction and unregisters itself on destruction.
 *
 * Thread safety: All methods are thread-safe (internally synchronized).
 */
class log_capture : public absl::LogSink {
public:
  log_capture();
  ~log_capture() override;

  log_capture(const log_capture&) = delete;
  log_capture& operator=(const log_capture&) = delete;
  log_capture(log_capture&&) = delete;
  log_capture& operator=(log_capture&&) = delete;

  /**
   * @brief Records an incoming Abseil log entry.
   */
  void Send(const absl::LogEntry& entry) override;

  /**
   * @brief Returns all captured messages matching the given severity.
   */
  [[nodiscard]] std::vector<std::string>
  messages(absl::LogSeverity severity) const;

  /**
   * @brief Returns all captured INFO log messages.
   */
  [[nodiscard]] std::vector<std::string> infos() const;

  /**
   * @brief Returns all captured WARNING log messages.
   */
  [[nodiscard]] std::vector<std::string> warnings() const;

  /**
   * @brief Returns all captured ERROR log messages.
   */
  [[nodiscard]] std::vector<std::string> errors() const;

  /**
   * @brief Returns all captured log messages regardless of severity.
   */
  [[nodiscard]] std::vector<std::string> all_messages() const;

  /**
   * @brief Clears all captured log entries.
   */
  void clear();

private:
  struct captured_entry {
    absl::LogSeverity severity;
    std::string message;
  };

  mutable std::mutex _mutex;
  std::vector<captured_entry> _entries;
};

} // namespace howling::testing
