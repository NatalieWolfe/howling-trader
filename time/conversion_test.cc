#include "time/conversion.h"

#include <chrono>

#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using ::std::chrono::duration_cast;
using ::std::chrono::microseconds;
using ::std::chrono::system_clock;

TEST(ToStdChronoTest, AbslTime) {
  absl::Time absl_time = absl::FromUnixMicros(123456789);
  system_clock::time_point time_point = to_std_chrono(absl_time);
  EXPECT_EQ(system_clock::to_time_t(time_point), 123);
  EXPECT_EQ(
      duration_cast<microseconds>(time_point.time_since_epoch()).count(),
      123456789);
}

TEST(ToStdChronoTest, ProtoTimestamp) {
  Timestamp proto_timestamp;
  proto_timestamp.set_seconds(123);
  proto_timestamp.set_nanos(456000);
  system_clock::time_point time_point = to_std_chrono(proto_timestamp);
  EXPECT_EQ(
      duration_cast<microseconds>(time_point.time_since_epoch()).count(),
      123000456);
}

TEST(ToStdChronoTest, AbslDuration) {
  absl::Duration absl_duration = absl::Microseconds(123456);
  microseconds chrono_duration = to_std_chrono(absl_duration);
  EXPECT_EQ(chrono_duration.count(), 123456);
}

TEST(ToStdChronoTest, ProtoDuration) {
  Duration proto_duration;
  proto_duration.set_seconds(123);
  proto_duration.set_nanos(456000);
  microseconds chrono_duration = to_std_chrono(proto_duration);
  EXPECT_EQ(chrono_duration.count(), 123000456);
}

TEST(ToProtoTest, TimePoint) {
  system_clock::time_point time_point =
      system_clock::from_time_t(123) + microseconds(456);
  Timestamp proto_timestamp = to_proto(time_point);
  EXPECT_EQ(proto_timestamp.seconds(), 123);
  EXPECT_EQ(proto_timestamp.nanos(), 456000);
}

TEST(ToProtoTest, AbslDuration) {
  absl::Duration absl_duration = absl::Microseconds(123456);
  Duration proto_duration = to_proto(absl_duration);
  EXPECT_EQ(proto_duration.seconds(), 0);
  EXPECT_EQ(proto_duration.nanos(), 123456000);
}

TEST(ToProtoTest, ChronoDuration) {
  microseconds chrono_duration(123456789);
  Duration proto_duration = to_proto(chrono_duration);
  EXPECT_EQ(proto_duration.seconds(), 123);
  EXPECT_EQ(proto_duration.nanos(), 456789000);
}

} // namespace
} // namespace howling
