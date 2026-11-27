#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "data/wilder_rma.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * @brief Configuration parameters for RSI period and thresholds.
 */
struct rsi_options {
  // Number of price-change bars used for Wilder's RMA smoothing of average
  // gains and losses. Must be positive.
  int period = 14;
  // RSI reading below which the symbol is treated as oversold, emitting a BUY
  // (or HOLD) signal. Must satisfy 0 < oversold < overbought < 100.
  double oversold_threshold = 30.0;
  // RSI reading above which the symbol is treated as overbought, emitting a
  // SELL (or HOLD) signal. Must satisfy 0 < oversold < overbought < 100.
  double overbought_threshold = 70.0;
};

/**
 * @brief Momentum oscillator analyzer implementing Wilder's Relative Strength
 * Index (RSI).
 *
 * Emits BUY (or HOLD if unable to buy) when RSI falls below the oversold
 * threshold, SELL (or HOLD if unable to sell) when RSI rises above the
 * overbought threshold, and NO_ACTION during warmup, neutral readings, or
 * non-step minutes on multi-minute timeframes.
 *
 * Thread-compatible: external synchronization is required for concurrent
 * access.
 */
class rsi_analyzer : public analyzer {
public:
  using options = rsi_options;

  /**
   * @brief Constructs an RSI analyzer with the given aggregation timeframe and
   * options.
   *
   * @param candle_period
   *  Member pointer selecting the aggregation series to analyze.
   * @param opts
   *  RSI period and overbought/oversold threshold configuration.
   * @throws std::invalid_argument if opts.period <= 0, candle_period is null,
   *  or thresholds do not satisfy 0 < oversold < overbought < 100.
   */
  explicit rsi_analyzer(
      vector<window> aggregations::* candle_period = &aggregations::one_minute,
      options opts = {});

  decision analyze(stock::Symbol symbol, const trading_state& data) override;

  /**
   * @brief Returns the most recently computed RSI value for the given symbol,
   * or std::nullopt if the symbol has not completed warmup.
   */
  [[nodiscard]] std::optional<double> current_rsi(stock::Symbol symbol) const;

private:
  struct _symbol_state {
    explicit _symbol_state(int period)
        : average_gain(period), average_loss(period) {}

    size_t last_processed_count = 0;
    std::optional<double> previous_close;
    wilder_rma average_gain;
    wilder_rma average_loss;
    std::optional<double> last_rsi;
  };

  static void _update_state(_symbol_state& state, const vector<window>& series);

  options _options;
  vector<window> aggregations::* _candle_period;
  int _minimum_window_size;
  std::unordered_map<stock::Symbol, _symbol_state> _symbol_states;
};

} // namespace howling
