#include "data/analyzers/howling.h"

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

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

} // namespace

howling_analyzer::howling_analyzer()
    : _macd1(&aggregations::one_minute), _macd5(&aggregations::five_minute) {}

decision
howling_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  auto market_hours = _market_hours(symbol, data);
  if (market_hours.confidence >= 0.98) return market_hours;

  deciders buy, sell, hold;
  add(buy, sell, hold, market_hours);
  // TODO: Consider how to incorporate confidence level better. For now, the
  // manual evaluation below is doing better.

  if (buy.confidence() > (sell.confidence() + hold.confidence())) {
    return {.act = action::BUY, .confidence = buy.confidence()};
  }
  if (sell.confidence() > (buy.confidence() + hold.confidence())) {
    return {.act = action::SELL, .confidence = sell.confidence()};
  }
  if (hold.confidence() > (buy.confidence() + sell.confidence())) {
    return {.act = action::HOLD, .confidence = hold.confidence()};
  }

  decision macd5 = _macd5(symbol, data);
  decision macd1 = _macd1(symbol, data);
  if (macd1.act == action::SELL) {
    if (macd5.act == action::HOLD && macd5.confidence > macd1.confidence) {
      return macd5;
    }
    return can_sell(symbol, data) ? macd1 : NO_ACTION;
  }
  if (macd5.act == action::BUY) {
    return can_buy(symbol, data) ? macd5 : NO_ACTION;
  }

  return NO_ACTION;
}

} // namespace howling
