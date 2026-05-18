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
  absl::Time t = absl::FromUnixMicros(123456789);
  system_clock::time_point tp = to_std_chrono(t);
  EXPECT_EQ(system_clock::to_time_t(tp), 123);
  EXPECT_EQ(
      duration_cast<microseconds>(tp.time_since_epoch()).count(), 123456789);
}

TEST(ToStdChronoTest, ProtoTimestamp) {
  Timestamp ts;
  ts.set_seconds(123);
  ts.set_nanos(456000);
  system_clock::time_point tp = to_std_chrono(ts);
  EXPECT_EQ(
      duration_cast<microseconds>(tp.time_since_epoch()).count(), 123000456);
}

TEST(ToStdChronoTest, AbslDuration) {
  absl::Duration d = absl::Microseconds(123456);
  microseconds ms = to_std_chrono(d);
  EXPECT_EQ(ms.count(), 123456);
}

TEST(ToStdChronoTest, ProtoDuration) {
  Duration d;
  d.set_seconds(123);
  d.set_nanos(456000);
  microseconds ms = to_std_chrono(d);
  EXPECT_EQ(ms.count(), 123000456);
}

TEST(ToProtoTest, TimePoint) {
  system_clock::time_point tp =
      system_clock::from_time_t(123) + microseconds(456);
  Timestamp ts = to_proto(tp);
  EXPECT_EQ(ts.seconds(), 123);
  EXPECT_EQ(ts.nanos(), 456000);
}

TEST(ToProtoTest, AbslDuration) {
  absl::Duration d = absl::Microseconds(123456);
  Duration proto_d = to_proto(d);
  EXPECT_EQ(proto_d.seconds(), 0);
  EXPECT_EQ(proto_d.nanos(), 123456000);
}

TEST(ToProtoTest, ChronoDuration) {
  microseconds d(123456789);
  Duration proto_d = to_proto(d);
  EXPECT_EQ(proto_d.seconds(), 123);
  EXPECT_EQ(proto_d.nanos(), 456789000);
}

} // namespace
} // namespace howling
