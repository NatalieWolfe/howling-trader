#pragma once

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * Advises SELL if the current price of the stock is greater than the lowest
 * acquired price.
 */
class profit_analyzer : public analyzer {
public:
  decision analyze(stock::Symbol symbol, const trading_state& data) override;
};

} // namespace howling
