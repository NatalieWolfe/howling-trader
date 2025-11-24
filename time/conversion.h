#pragma once

#include <chrono>

#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"

namespace howling {

std::chrono::system_clock::time_point to_std_chrono(absl::Time time);
std::chrono::system_clock::time_point
to_std_chrono(const google::protobuf::Timestamp& time);

std::chrono::microseconds to_std_chrono(absl::Duration duration);
std::chrono::microseconds
to_std_chrono(const google::protobuf::Duration& duration);

google::protobuf::Timestamp
to_proto(std::chrono::system_clock::time_point time);

google::protobuf::Duration to_proto(absl::Duration duration);

} // namespace howling
