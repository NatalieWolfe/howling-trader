#pragma once

#include <format>
#include <optional>

#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "trading/metrics.h"
#include "trading/trading_state.h"

namespace howling {

struct print_candle_parameters {
  double last_buy_price = 0.0;
  double price_min = 0.0;
  double price_max = 0.0;
  double candle_print_min = 0.0;
  double candle_print_max = 300.0;
  double candle_width = 0.7;
};

std::string print_candle(
    const decision& d,
    const std::optional<trading_state::position>& trade,
    const Candle& candle,
    const print_candle_parameters& params);

inline std::string print_price(double price) {
  return std::format("{:.2f}", price);
}

std::string print_metrics(const metrics& m);

} // namespace howling
