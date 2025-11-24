#pragma once

#include "containers/vector.h"
#include "data/analyzer.h"
#include "data/candle.pb.h"

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

  explicit zig_zag_analyzer(const vector<Candle>& full_history, options opts);

  decision analyze(const aggregations& data) override;

private:
  vector<Candle> _buy_points;
  vector<Candle> _sell_points;
};

} // namespace howling
