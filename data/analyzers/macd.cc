#include "data/analyzers/macd.h"

#include <algorithm>

#include "absl/flags/flag.h"
#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

ABSL_FLAG(
    double,
    macd_crossover_scaler,
    5.0, // 5x scaler means 20c shift is 1.0 confidence.
    "Multiplier on crossover slope to calculate confidence.");

namespace howling {

decision macd_crossover_analyzer::analyze(
    stock::Symbol symbol, const trading_state& data) {
  const vector<window>& period = data.market.at(symbol).*_period;
  if (period.size() < 2) {
    return {.act = action::NO_ACTION, .confidence = 0.0};
  }

  const window& current = period(-1);
  const window& previous = period(-2);

  double current_delta = current.macd_fast_line - current.macd_signal_line;
  double previous_delta = previous.macd_fast_line - previous.macd_signal_line;
  double delta_slope = current_delta - previous_delta;

  if (current_delta > 0.0 && previous_delta <= 0.0) {
    return {
        .act = can_buy(symbol, data) ? action::BUY : action::HOLD,
        .confidence = std::min(
            1.0, delta_slope * absl::GetFlag(FLAGS_macd_crossover_scaler))};
  }
  if (current_delta < 0.0 && previous_delta >= 0.0) {
    return {
        .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
        .confidence = std::min(
            1.0, -delta_slope * absl::GetFlag(FLAGS_macd_crossover_scaler))};
  }
  if (current_delta > 0 && previous_delta > 0 && delta_slope > 0) {
    return {.act = action::HOLD, .confidence = delta_slope / current.count};
  }
  return {.act = action::NO_ACTION, .confidence = 0.0};
}

} // namespace howling
