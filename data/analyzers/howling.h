#pragma once

#include "containers/circular_buffer.h"
#include "data/analyzer.h"
#include "data/analyzers/bollinger.h"
#include "data/analyzers/macd.h"
#include "data/analyzers/market_hours.h"
#include "data/analyzers/profit.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * Combination analyzer bringing in different signals.
 */
class howling_analyzer : public analyzer {
public:
  howling_analyzer();

  decision analyze(stock::Symbol symbol, const trading_state& data) override;

private:
  market_hours_analyzer _market_hours;
  bollinger_analyzer _bollinger;
  macd_crossover_analyzer _macd1;
  macd_crossover_analyzer _macd5;
  profit_analyzer _profit;

  circular_buffer<decision> _macd1_decisions;
  circular_buffer<decision> _macd5_decisions;
};

} // namespace howling
