#pragma once

#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

enum class action {
  // Null action.
  NO_ACTION = 0,
  // Do not buy, do not sell.
  HOLD = 1,
  // Buy shares of the stock.
  BUY = 2,
  // Sell shares of the stock.
  SELL = 3
};

struct decision {
  action act;
  double confidence;
};

constexpr decision NO_ACTION = {.act = action::NO_ACTION, .confidence = 0.0};

class analyzer {
public:
  virtual ~analyzer() = default;

  virtual decision analyze(stock::Symbol symbol, const trading_state& data) = 0;

  decision operator()(stock::Symbol symbol, const trading_state& data) {
    return analyze(symbol, data);
  }

protected:
  /**
   * Returns true if there are enough available funds to buy a single share of
   * the given stock at its current low.
   */
  bool can_buy(stock::Symbol symbol, const trading_state& data) const;

  /**
   * Returns true if any positions of the given stock are currently held.
   */
  bool can_sell(stock::Symbol symbol, const trading_state& data) const;
};

} // namespace howling
