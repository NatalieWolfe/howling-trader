#pragma once

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * Watches for movements outside of the Bollinger bands.
 */
class bollinger_analyzer : public analyzer {
public:
  decision analyze(stock::Symbol symbol, const trading_state& data) override;
};

} // namespace howling
