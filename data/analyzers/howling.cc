#include "data/analyzers/howling.h"

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {
namespace {

struct deciders {
  int count = 0;
  double total_confidence = 0.0;
  double confidence() const { return count ? total_confidence / count : 0.0; }
};

void add(deciders& d, const decision& new_decision) {
  ++d.count;
  d.total_confidence += new_decision.confidence;
}

void add(
    deciders& buy,
    deciders& sell,
    deciders& hold,
    const decision& new_decision) {
  if (new_decision.act == action::BUY) {
    add(buy, new_decision);
  } else if (new_decision.act == action::SELL) {
    add(sell, new_decision);
  } else if (new_decision.act == action::HOLD) {
    add(hold, new_decision);
  }
}

decision get_top_decision(const circular_buffer<decision>& decisions) {
  deciders buy, sell, hold;
  for (const decision& d : decisions) add(buy, sell, hold, d);

  if (buy.confidence() > sell.confidence() &&
      buy.confidence() > hold.confidence()) {
    return {.act = action::BUY, .confidence = buy.confidence()};
  }
  if (sell.confidence() > buy.confidence() &&
      sell.confidence() > hold.confidence()) {
    return {.act = action::SELL, .confidence = sell.confidence()};
  }
  if (hold.confidence() > sell.confidence() &&
      hold.confidence() > buy.confidence()) {
    return {.act = action::HOLD, .confidence = hold.confidence()};
  }
  // No clear confidence.
  return NO_ACTION;
}

} // namespace

howling_analyzer::howling_analyzer()
    : _macd1(&aggregations::one_minute), _macd5(&aggregations::five_minute),
      _macd1_decisions{5}, _macd5_decisions{5} {}

decision
howling_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  auto market_hours = _market_hours(symbol, data);
  if (market_hours.confidence >= 0.98) return market_hours;

  decision macd1 = _macd1(symbol, data);
  decision macd5 = _macd5(symbol, data);
  decision boll = _bollinger(symbol, data);
  decision profit = _profit(symbol, data);

  _macd1_decisions.push_back(macd1);
  _macd5_decisions.push_back(macd5);
  if (_macd5_decisions.size() < 5) return NO_ACTION;

  decision top_macd5 = get_top_decision(_macd5_decisions);

  if (can_sell(symbol, data)) {
    // Near market close.
    if (market_hours.act == action::SELL && market_hours.confidence > 0.5) {
      return market_hours;
    }

    // Profit taking - lock in gains if profit is substantial.
    if (profit.act == action::SELL && profit.confidence > 0.8) return profit;

    // MACD 1m crossover sell.
    if (macd1.act == action::SELL && macd1.confidence > 0.1) return macd1;

    // Bollinger overbought - exit if price is pushing upper band and MACD
    // confirms downtrend.
    if (boll.act == action::SELL &&
        (macd1.act == action::SELL || macd1.act == action::HOLD)) {
      return boll;
    }
  }

  if (can_buy(symbol, data)) {
    // Bollinger oversold + MACD turning up.
    if (boll.act == action::BUY && macd1.confidence > 0.05) return boll;

    // MACD 1m crossover buy, confirmed by 5m trend and NOT overbought.
    // Only buy if we aren't already near the upper Bollinger band.
    if (macd1.act == action::BUY && macd1.confidence > 0.1 &&
        top_macd5.act != action::SELL && boll.act != action::SELL) {
      return macd1;
    }
  }

  return NO_ACTION;
}

} // namespace howling
