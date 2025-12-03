#include "data/analyzers/macd.h"

#include <algorithm>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

ABSL_FLAG(
    double,
    macd_crossover_scaler,
    5.0, // 5x scaler means 0.20 shift is 1.0 confidence.
    "Multiplier on crossover slope to calculate confidence.");

namespace howling {

decision macd_crossover_analyzer::analyze(
    stock::Symbol symbol, const trading_state& data) {
  const vector<window>& period = data.market.at(symbol).*_period;
  const int window_size = period(-1).count;
  if (period.size() < window_size * 2 ||
      data.market_minute() % window_size != 0) {
    return NO_ACTION;
  }

  const window& current = period(-1);
  const window& previous = period(-window_size - 1);

  double current_delta = current.macd_fast_line - current.macd_signal_line;
  double previous_delta = previous.macd_fast_line - previous.macd_signal_line;
  double delta_slope = current_delta - previous_delta;

  // Cross under.
  if (current_delta < 0.0 && previous_delta >= 0.0) {
    // LOG(INFO) << data.market_hour() << ":" << data.market_minute() << " ["
    //           << window_size << "]"
    //           << " SELL: " << current_delta << " : " << previous_delta << " :
    //           "
    //           << delta_slope;
    return {
        .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
        .confidence = std::min(
            1.0, -delta_slope * absl::GetFlag(FLAGS_macd_crossover_scaler))};
  }
  // Cross over.
  if (current_delta > 0.0 && previous_delta <= 0.0) {
    // LOG(INFO) << data.market_hour() << ":" << data.market_minute() << " ["
    //           << window_size << "]"
    //           << " BUY: " << current_delta << " : " << previous_delta << " :
    //           "
    //           << delta_slope;
    return {
        .act = can_buy(symbol, data) ? action::BUY : action::HOLD,
        .confidence = std::min(
            1.0, delta_slope * absl::GetFlag(FLAGS_macd_crossover_scaler))};
  }
  // Building upward momentum.
  if (current_delta > 0.0 && previous_delta > 0.0 && delta_slope > 0.0) {
    // LOG(INFO) << data.market_hour() << ":" << data.market_minute() << " ["
    //           << window_size << "]"
    //           << " HOLD: " << current_delta << " : " << previous_delta << " :
    //           "
    //           << delta_slope;
    return {
        .act = action::HOLD,
        .confidence = delta_slope * absl::GetFlag(FLAGS_macd_crossover_scaler)};
  }
  // Not enough signal to advise.
  return NO_ACTION;
}

} // namespace howling
