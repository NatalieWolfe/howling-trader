#pragma once

#include <optional>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

struct cmf_options {
  int lookback_periods = 21;
  // Deadzone around zero to filter noise (CMF is bounded in [-1.0, 1.0]).
  double threshold = 0.05;
  // Multiplier applied to signal magnitude to compute decision confidence.
  double confidence_scaler = 2.0;
  // Enable bearish price-vs-CMF divergence detection.
  bool enable_divergence_check = true;
};

/**
 * @brief Chaikin Money Flow (CMF) analyzer measuring volume-weighted
 * accumulation and distribution over a rolling lookback window.
 *
 * Samples non-overlapping bars at stride `window_size` from the configured
 * rolling aggregation series (`one_minute`, `five_minute`, or `twenty_minute`)
 * and evaluates continuously once enough history is available.
 *
 * Thread-safe: holds only immutable configuration and no mutable state.
 */
class cmf_analyzer : public analyzer {
public:
  using options = cmf_options;

  /**
   * @brief Constructs a Chaikin Money Flow analyzer.
   *
   * @param period
   *  Pointer to the aggregation window series member to sample.
   * @param opts
   *  Configuration options for lookback length, deadzone threshold, confidence
   *  scaling, and bearish divergence detection.
   * @throws std::invalid_argument if period is null or any option is invalid.
   */
  explicit cmf_analyzer(
      vector<window> aggregations::* period = &aggregations::one_minute,
      options opts = {});

  /**
   * @brief Analyzes the current trading state for the given symbol and returns
   * a trading decision based on CMF accumulation, distribution, or bearish
   * divergence.
   */
  decision analyze(stock::Symbol symbol, const trading_state& data) override;

  /**
   * @brief Calculates the Chaikin Money Flow value in [-1.0, 1.0] for the
   * given symbol over the configured lookback window.
   *
   * @param symbol Stock symbol to evaluate.
   * @param data Current trading state containing market aggregations.
   * @return The CMF value, or std::nullopt if data is missing, history is
   *  insufficient, or total lookback volume is non-positive.
   */
  [[nodiscard]] std::optional<double>
  calculate_cmf(stock::Symbol symbol, const trading_state& data) const;

private:
  [[nodiscard]] std::optional<double> _calculate_cmf_at(
      const aggregations& symbol_aggregation,
      int offset_bars,
      int window_size) const;

  [[nodiscard]] std::optional<double> _detect_bearish_divergence(
      const aggregations& symbol_aggregation,
      double current_cmf,
      int window_size) const;

  vector<window> aggregations::* _period;
  options _options;
};

} // namespace howling
