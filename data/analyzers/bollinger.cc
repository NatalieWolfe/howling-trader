#include "data/analyzers/bollinger.h"

#include <unordered_map>

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

decision
bollinger_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  const aggregations& market = data.market.at(symbol);
  if (market.one_minute.size() < 20) {
    // Need a minimum of 20 minutes of data to assess the bands.
    return {.act = action::NO_ACTION, .confidence = 0.0};
  }

  if (market.one_minute(-1).candle.high() >
          market.twenty_minute(-1).upper_bollinger_band &&
      can_sell(symbol, data)) {
    return {.act = action::SELL, .confidence = 1.0};
  }
  if (market.one_minute(-1).candle.low() <
          market.twenty_minute(-1).lower_bollinger_band &&
      can_buy(symbol, data)) {
    return {.act = action::BUY, .confidence = 1.0};
  }
  return {.act = action::HOLD, .confidence = 1.0};
}

} // namespace howling
