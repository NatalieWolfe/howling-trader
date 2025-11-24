#pragma once

#include "containers/vector.h"
#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {

/**
 * An omniscient analyzer that will make optimal trades.
 *
 * Must be given the full-day's stock changes before running an analysis.
 */
class zig_zag_analyzer : public analyzer {
public:
  struct options {
    double threshold = 0.5;
  };

  explicit zig_zag_analyzer(const stock::History& full_history, options opts);

  std::unordered_map<stock::Symbol, decision>
  analyze(const trading_state& data) override;

private:
  // TODO: Extend this analyzer to support multiple stocks at once.
  stock::Symbol _symbol;
  vector<Candle> _buy_points;
  vector<Candle> _sell_points;
};

} // namespace howling
