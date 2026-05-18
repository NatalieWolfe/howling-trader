#pragma once

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * Looks for the crossover points in the MACD momentum trend.
 */
class macd_crossover_analyzer : public analyzer {
public:
  macd_crossover_analyzer(vector<window> aggregations::* period)
      : _period{period} {}

  decision analyze(stock::Symbol symbol, const trading_state& data) override;

private:
  vector<window> aggregations::* _period;
};

} // namespace howling
