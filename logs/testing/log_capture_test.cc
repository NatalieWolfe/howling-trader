#include "logs/testing/log_capture.h"

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling::testing {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(LogCaptureTest, CapturesMessagesBySeverityAndClears) {
  log_capture capture;

  LOG(INFO) << "info message";
  LOG(WARNING) << "warning message";
  LOG(ERROR) << "error message";

  EXPECT_THAT(capture.infos(), ElementsAre("info message"));
  EXPECT_THAT(capture.warnings(), ElementsAre("warning message"));
  EXPECT_THAT(capture.errors(), ElementsAre("error message"));
  EXPECT_THAT(
      capture.messages(absl::LogSeverity::kWarning),
      ElementsAre("warning message"));
  EXPECT_THAT(
      capture.all_messages(),
      ElementsAre("info message", "warning message", "error message"));

  capture.clear();
  EXPECT_THAT(capture.all_messages(), IsEmpty());
  EXPECT_THAT(capture.infos(), IsEmpty());
  EXPECT_THAT(capture.warnings(), IsEmpty());
  EXPECT_THAT(capture.errors(), IsEmpty());
}

} // namespace
} // namespace howling::testing
