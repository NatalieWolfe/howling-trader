#include "logs/stdout_sink.h"

#include <iostream>

#include "absl/flags/flag.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"

ABSL_FLAG(bool, logging_always_flush, false, "Always flush log output");

namespace howling {

void StdoutLogSink::Send(const absl::LogEntry& entry) {
  std::cout << entry.text_message_with_prefix_and_newline();
  if (absl::GetFlag(FLAGS_logging_always_flush)) Flush();
}

void StdoutLogSink::Flush() {
  std::cout << std::flush;
}

} // namespace howling
