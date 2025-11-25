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
  add(buy, sell, hold, _bollinger(symbol, data));
  add(buy, sell, hold, _macd1(symbol, data));

  // TODO: Determine how best to incorporate time-shifted signals. MACD1 cross
  // happens before MACD5, so the current pattern would issue 2 buys. Looking
  // at one or the other gives decent results, but is risky because a combined
  // signal is a stronger signal.
  // add(buy, sell, hold, _macd5(symbol, data));

  if (buy.confidence() > (sell.confidence() + hold.confidence())) {
    return {.act = action::BUY, .confidence = buy.confidence()};
  }
  if (sell.confidence() > (buy.confidence() + hold.confidence())) {
    return {.act = action::SELL, .confidence = sell.confidence()};
  }
  if (hold.confidence() > (buy.confidence() + sell.confidence())) {
    return {.act = action::HOLD, .confidence = hold.confidence()};
  }

  return {.act = action::NO_ACTION, .confidence = 0.0};
}

} // namespace howling
