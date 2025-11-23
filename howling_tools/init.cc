#include "howling_tools/init.h"

#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/log/log_sink_registry.h"
#include "howling_tools/runfiles_init.h"
#include "logs/stdout_sink.h"

namespace howling {

void init(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  static StdoutLogSink log_sink;
  absl::AddLogSink(&log_sink);
  initialize_runfiles(argv[0]);
}

} // namespace howling
