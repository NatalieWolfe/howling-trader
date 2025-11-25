#include "data/analyzers/market_hours.h"

#include "absl/flags/flag.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

ABSL_FLAG(int, market_hours_exit, 14, "Hour of day to call it quits and dump.");

namespace howling {

decision market_hours_analyzer::analyze(
    stock::Symbol symbol, const trading_state& data) {
  action a = (data.market_is_open() &&
              data.market_hour() < absl::GetFlag(FLAGS_market_hours_exit))
      ? action::NO_ACTION
      : action::SELL;

  if (a == action::NO_ACTION) {
    return {.act = action::NO_ACTION, .confidence = 0.0};
  }
  if (can_sell(symbol, data)) {
    return {.act = action::SELL, .confidence = 1.0};
  }
  return {.act = action::HOLD, .confidence = 1.0};
}

} // namespace howling
