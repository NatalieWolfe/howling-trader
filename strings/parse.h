#pragma once

#include <chrono>
#include <string_view>

namespace howling {

std::chrono::system_clock::time_point
parse_timepoint(std::string_view timepoint_str);

std::chrono::milliseconds parse_duration(std::string_view duration_str);

std::chrono::milliseconds parse_gap(std::string_view duration_str);

int parse_int(std::string_view int_str);

double parse_double(std::string_view double_str);

} // namespace howling
