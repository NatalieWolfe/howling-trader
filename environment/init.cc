#include "environment/init.h"

#include <string>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log_sink_registry.h"
#include "environment/runfiles_init.h"
#include "logs/json_sink.h"
#include "logs/stdout_sink.h"

ABSL_FLAG(
    std::string, logging_mode, "standard", "Logging mode (standard, json)");

namespace howling {

void init(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);
  if (absl::GetFlag(FLAGS_logging_mode) == "json") {
    static JsonLogSink* json_sink = new JsonLogSink();
    absl::AddLogSink(json_sink);
  } else {
    static StdoutLogSink* stdout_sink = new StdoutLogSink();
    absl::AddLogSink(stdout_sink);
  }
  initialize_runfiles(argv[0]);
}

} // namespace howling
