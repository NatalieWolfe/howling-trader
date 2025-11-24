#include "time/conversion.h"

#include <chrono>

#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/time_util.h"

namespace howling {

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using ::google::protobuf::util::TimeUtil;
using ::std::chrono::microseconds;
using ::std::chrono::system_clock;

system_clock::time_point to_std_chrono(absl::Time time) {
  return absl::ToChronoTime(time);
}

system_clock::time_point to_std_chrono(const Timestamp& time) {
  return system_clock::time_point{
      microseconds{TimeUtil::TimestampToMicroseconds(time)}};
}

microseconds to_std_chrono(absl::Duration duration) {
  return absl::ToChronoMicroseconds(duration);
}

microseconds to_std_chrono(const Duration& duration) {
  return microseconds{TimeUtil::DurationToMicroseconds(duration)};
}

Timestamp to_proto(system_clock::time_point time) {
  using namespace ::std::chrono;
  return TimeUtil::MicrosecondsToTimestamp(
      duration_cast<microseconds>(time.time_since_epoch()).count());
}

Duration to_proto(absl::Duration duration) {
  return TimeUtil::MicrosecondsToDuration(absl::ToInt64Microseconds(duration));
}

} // namespace howling
