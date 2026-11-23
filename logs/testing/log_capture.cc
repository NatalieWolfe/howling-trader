#include "logs/testing/log_capture.h"

#include <mutex>
#include <string>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink_registry.h"

namespace howling::testing {

log_capture::log_capture() {
  absl::AddLogSink(this);
}

log_capture::~log_capture() {
  absl::RemoveLogSink(this);
}

void log_capture::Send(const absl::LogEntry& entry) {
  std::lock_guard<std::mutex> lock(_mutex);
  _entries.push_back(
      captured_entry{
          .severity = entry.log_severity(),
          .message = std::string(entry.text_message()),
      });
}

std::vector<std::string>
log_capture::messages(absl::LogSeverity severity) const {
  std::lock_guard<std::mutex> lock(_mutex);
  std::vector<std::string> result;
  for (const captured_entry& entry : _entries) {
    if (entry.severity == severity) result.push_back(entry.message);
  }
  return result;
}

std::vector<std::string> log_capture::infos() const {
  return messages(absl::LogSeverity::kInfo);
}

std::vector<std::string> log_capture::warnings() const {
  return messages(absl::LogSeverity::kWarning);
}

std::vector<std::string> log_capture::errors() const {
  return messages(absl::LogSeverity::kError);
}

std::vector<std::string> log_capture::all_messages() const {
  std::lock_guard<std::mutex> lock(_mutex);
  std::vector<std::string> result;
  result.reserve(_entries.size());
  for (const captured_entry& entry : _entries) result.push_back(entry.message);
  return result;
}

void log_capture::clear() {
  std::lock_guard<std::mutex> lock(_mutex);
  _entries.clear();
}

} // namespace howling::testing
