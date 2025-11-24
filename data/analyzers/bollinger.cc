#include "data/analyzers/bollinger.h"

#include <unordered_map>

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

std::unordered_map<stock::Symbol, decision>
bollinger_analyzer::analyze(const trading_state& data) {
  std::unordered_map<stock::Symbol, decision> decisions;
  for (stock::Symbol symbol : data.available_stocks) {
    const aggregations& market = data.market.at(symbol);
    if (market.one_minute.size() < 2) continue;

    if (market.one_minute(-1).candle.high() >
            market.twenty_minute(-2).upper_bollinger_band &&
        can_sell(symbol, data)) {
      decisions[symbol].act = action::SELL;
      decisions[symbol].confidence = 1.0;
    } else if (
        market.one_minute(-1).candle.low() <
            market.twenty_minute(-2).lower_bollinger_band &&
        can_buy(symbol, data)) {
      decisions[symbol].act = action::BUY;
      decisions[symbol].confidence = 1.0;
    } else {
      decisions[symbol].act = action::HOLD;
      decisions[symbol].confidence = 1.0;
    }
  }
  return decisions;
}

} // namespace howling
