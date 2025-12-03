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

decision get_top_decision(const circular_buffer<decision>& decisions) {
  deciders buy, sell, hold;
  for (const decision& d : decisions) {
    add(buy, sell, hold, d);
  }

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

bool confident(const decision& d, action a, double confidence = 0.75) {
  return d.act == a && d.confidence >= confidence;
}

} // namespace

howling_analyzer::howling_analyzer()
    : _macd1(&aggregations::one_minute), _macd5(&aggregations::five_minute),
      _macd1_decisions{5}, _macd5_decisions{5} {}

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
  _macd1_decisions.push_back(macd1);
  _macd5_decisions.push_back(macd5);
  if (_macd5_decisions.size() < 5) return NO_ACTION;

  decision top_macd5 = get_top_decision(_macd5_decisions);

  if (macd1.act == action::SELL) {
    if (top_macd5.act == action::HOLD &&
        top_macd5.confidence > macd1.confidence) {
      return macd5;
    }
    if (top_macd5.act != action::HOLD ||
        confident(_profit(symbol, data), action::SELL)) {
      return can_sell(symbol, data) ? macd1 : NO_ACTION;
    }
    return NO_ACTION;
  }
  if (confident(macd5, action::BUY, 0.25)) {
    return can_buy(symbol, data) ? macd5 : NO_ACTION;
  }
  if ((!data.positions.contains(symbol) || data.positions.at(symbol).empty()) &&
      confident(top_macd5, action::HOLD, 0.1) &&
      confident(macd1, action::BUY, 0.1)) {
    return can_buy(symbol, data) ? macd1 : NO_ACTION;
  }

  // Notes for possible directions to take this:
  //
  // Previous day market green + pre-market 4 hours green, today likely green
  // Try to enter early this day.
  //
  // Previous day market green + pre-market 4 hours red, sentiment change.
  // Enter earlier because start of day may be drop vs rest of day.
  //
  // First 5 minute bar (maybe 15 minute bar?) with short wick set stop-loss at
  // half of bar if entered during this bar.
  //
  // First 5 minute bar with long bottom wick (hanging man), set stop-loss at
  // bottom of wick if entered during that bar.
  //
  // Potential new analyzer looking at any wick on top (japanese word for bald).

  return NO_ACTION;
}

} // namespace howling
