#include "logs/stdout_sink.h"

#include <iostream>

#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"

namespace howling {

void StdoutLogSink::Send(const absl::LogEntry& entry) {
  std::cout << entry.text_message_with_prefix_and_newline();
}

void StdoutLogSink::Flush() { std::cout << std::flush; }

} // namespace howling
