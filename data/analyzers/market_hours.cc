#include "data/analyzers/market_hours.h"

#include <algorithm>

#include "absl/flags/flag.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

ABSL_FLAG(int, market_hours_exit, 14, "Hour of day to call it quits and dump.");

namespace howling {

constexpr double EJECT_TIME = 15.5; // 3:30.

decision market_hours_analyzer::analyze(
    stock::Symbol symbol, const trading_state& data) {
  int hour = data.market_hour();
  int minute = data.market_minute();

  // Don't do anything until the market is open for 30 minutes.
  if (hour < 10 || !data.market_is_open()) {
    return {.act = action::HOLD, .confidence = 1.0};
  }

  // No influence during business hours.
  if (hour < absl::GetFlag(FLAGS_market_hours_exit)) {
    return {.act = action::NO_ACTION, .confidence = 0.0};
  }

  // Nearing market close. Decide how urgently shares should be sold.
  double exit_window = EJECT_TIME - absl::GetFlag(FLAGS_market_hours_exit);
  double time_since_exit = static_cast<double>(hour) + (minute / 60.0) -
      absl::GetFlag(FLAGS_market_hours_exit);
  double confidence = std::min(1.0, time_since_exit / exit_window);

  // If we can sell shares, then sell. Otherwise, hold off on doing anything.
  return {
      .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
      .confidence = confidence};
}

} // namespace howling
