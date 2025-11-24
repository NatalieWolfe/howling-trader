#include "data/analyzers/market_hours.h"

#include <unordered_map>

#include "absl/flags/flag.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

ABSL_FLAG(int, market_hours_exit, 14, "Hour of day to call it quits and dump.");

namespace howling {

std::unordered_map<stock::Symbol, decision>
market_hours_analyzer::analyze(const trading_state& data) {
  std::unordered_map<stock::Symbol, decision> decisions;
  action a = (data.market_is_open() &&
              data.market_hour() < absl::GetFlag(FLAGS_market_hours_exit))
      ? action::NO_ACTION
      : action::SELL;

  for (stock::Symbol symbol : data.available_stocks) {
    if (a == action::NO_ACTION) {
      decisions[symbol].act = action::NO_ACTION;
      decisions[symbol].confidence = 0.0;
    } else if (can_sell(symbol, data)) {
      decisions[symbol].act = action::SELL;
      decisions[symbol].confidence = 1.0;
    } else {
      decisions[symbol].act = action::HOLD;
      decisions[symbol].confidence = 1.0;
    }
  }
  return decisions;
}

} // namespace howling
