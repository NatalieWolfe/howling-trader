#pragma once

#include <format>

#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "trading/metrics.h"

namespace howling {

struct print_extents {
  double min = 0.0;
  double max = 300.0;
  double max_width_usage = 1.0;
};

std::string print_candle(
    const decision& d,
    const metrics& m,
    const Candle& candle,
    const print_extents& candle_extents);

inline std::string print_price(double price) {
  return std::format("{:.2f}", price);
}

std::string print_metrics(const metrics& m);

} // namespace howling
