#pragma once

#include <optional>
#include <unordered_map>

#include "containers/circular_buffer.h"
#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/wilder_rma.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * @brief Configuration parameters for ATR calculation and spike detection.
 */
struct atr_options {
  int period = 14;
  int baseline_period = 60;
  double high_volatility_ratio = 1.5;
};

/**
 * @brief Volatility and risk-management analyzer based on Average True Range
 * (ATR) using Wilder's Running Moving Average.
 *
 * Emits action::HOLD when current ATR spikes above a configured multiple of its
 * rolling historical baseline, and action::NO_ACTION with risk-adjusted
 * confidence during normal volatility. Also exposes raw ATR and dynamic
 * stop-loss distance accessors for composition.
 *
 * Thread-compatible: external synchronization is required for concurrent
 * access.
 */
class atr_analyzer : public analyzer {
public:
  using options = atr_options;

  /**
   * @brief Constructs an ATR analyzer with the given timeframe and options.
   *
   * @param period
   *  Pointer to the aggregations window series member (for example,
   *  &aggregations::one_minute or &aggregations::five_minute).
   * @param opts
   *  Smoothing period, historical baseline window, and volatility spike ratio.
   * @throws std::invalid_argument if any option is non-positive/non-finite or
   *  if period is null.
   */
  explicit atr_analyzer(
      vector<window> aggregations::* period = &aggregations::one_minute,
      options opts = {});

  /**
   * @brief Updates ATR state for the symbol and returns a volatility decision.
   */
  decision analyze(stock::Symbol symbol, const trading_state& data) override;

  /**
   * @brief Returns the latest computed ATR for the symbol, or std::nullopt if
   * the symbol has not yet completed warmup.
   */
  [[nodiscard]] std::optional<double> current_atr(stock::Symbol symbol) const;

  /**
   * @brief Returns the ATR-scaled stop-loss distance (multiplier * current_atr)
   * for the symbol, or std::nullopt if the symbol has not yet completed warmup.
   *
   * @param symbol Stock symbol to query.
   * @param multiplier Positive finite ATR multiplier (defaults to 2.0).
   * @throws std::invalid_argument if multiplier is not positive and finite.
   */
  [[nodiscard]] std::optional<double>
  stop_loss_distance(stock::Symbol symbol, double multiplier = 2.0) const;

private:
  struct _symbol_state {
    _symbol_state(int period, int baseline_period);

    wilder_rma rma;
    circular_buffer<double> baseline;
    double baseline_sum = 0.0;
    std::optional<double> previous_close;
  };

  options _options;
  vector<window> aggregations::* _period;
  int _expected_window_size;
  std::unordered_map<stock::Symbol, _symbol_state> _states;
};

} // namespace howling
