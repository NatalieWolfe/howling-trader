#pragma once

#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"

namespace howling {

class JsonLogSink : public absl::LogSink {
public:
  void Send(const absl::LogEntry& entry) override;
  void Flush() override;
};

} // namespace howling
