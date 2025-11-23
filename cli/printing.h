#pragma once

#include "data/candle.pb.h"

namespace howling {

struct print_extents {
  double min = 0.0;
  double max = 300.0;
  double max_width_usage = 1.0;
};

std::string print_candle(const Candle& candle, const print_extents& extents);

} // namespace howling
