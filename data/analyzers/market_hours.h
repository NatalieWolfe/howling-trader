#pragma once

#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

/**
 * Analyzer looking at the operating time of the market.
 *
 * Issues HOLD outside of market hours, SELL when nearing closing, and NO_ACTION
 * during operating hours.
 */
class market_hours_analyzer : public analyzer {
public:
  decision analyze(stock::Symbol symbol, const trading_state& data) override;
};

} // namespace howling
