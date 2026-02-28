#include "environment/runfiles.h"

#include <string>
#include <string_view>

#include "absl/log/log.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace howling {
namespace {

using ::bazel::tools::cpp::runfiles::Runfiles;

Runfiles* make_runfiles(std::string_view argv0) {
  std::string error;
  Runfiles* runfiles = Runfiles::Create(std::string{argv0}, &error);
  if (runfiles == nullptr) {
    LOG(FATAL) << "Could not initialize runfiles: " << error;
  }
  return runfiles;
}

Runfiles& get_runfiles(std::string_view argv0 = "") {
  static std::string cached_argv0{argv0};
  static Runfiles* cached_runfiles = make_runfiles(cached_argv0);
  return *cached_runfiles;
}

} // namespace

void initialize_runfiles(std::string_view argv0) {
  get_runfiles(argv0);
}

std::string runfile(std::string_view path) {
  return get_runfiles().Rlocation(std::string{path});
}

} // namespace howling
