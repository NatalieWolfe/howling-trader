#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <unordered_map>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * @brief Signal generation mode for VWAP evaluation.
 */
enum class vwap_mode {
  // Follows intraday trend direction by signaling BUY when price crosses above
  // VWAP and SELL when price crosses below VWAP on consecutive session bars.
  TREND_FILTER,
  // Fades extended deviations from VWAP by signaling BUY when price falls
  // below VWAP by at least deviation_threshold and SELL when it rises above.
  MEAN_REVERSION,
};

/**
 * @brief Configuration options for `vwap_analyzer`.
 */
struct vwap_options {
  vwap_mode strategy_mode = vwap_mode::TREND_FILTER;
  double deviation_threshold = 0.002;
  int min_candles = 5;
  bool include_extended_hours = false;
};

/**
 * @brief Volume-Weighted Average Price (VWAP) analyzer with daily Eastern Time
 * session resets.
 *
 * Computes intraday cumulative VWAP using candle typical price
 * `(high + low + close) / 3` weighted by candle volume, resetting automatically
 * at each America/New_York trading day boundary. Supports trend-filter
 * crossover signals and mean-reversion deviation signals, and exposes
 * `current_vwap()` for composition in higher-level analyzers.
 *
 * Thread-compatible: external synchronization is required for concurrent
 * access.
 */
class vwap_analyzer : public analyzer {
public:
  using mode = vwap_mode;
  using options = vwap_options;

  /**
   * @brief Constructs a VWAP analyzer with the specified options.
   *
   * @param opts Configuration parameters for VWAP calculation.
   * @throws std::invalid_argument if `min_candles <= 0` or
   * `deviation_threshold` is negative or non-finite.
   */
  explicit vwap_analyzer(options opts = {});

  /**
   * @brief Updates intraday VWAP state for `symbol` and emits a trading
   * decision.
   *
   * @param symbol Stock symbol to analyze.
   * @param data Current trading state containing market data and account state.
   * @return Trading decision (`BUY`, `SELL`, or `NO_ACTION`).
   */
  decision analyze(stock::Symbol symbol, const trading_state& data) override;

  /**
   * @brief Returns the latest computed intraday VWAP for `symbol`.
   *
   * @param symbol Stock symbol to query.
   * @return Current session VWAP, or `std::nullopt` if no positive-volume
   * session candles have been processed for the active trading day.
   */
  [[nodiscard]] std::optional<double> current_vwap(stock::Symbol symbol) const;

private:
  struct _session_state {
    std::optional<std::chrono::local_days> session_day;
    std::size_t last_processed_count = 0;
    int session_candle_count = 0;
    double cumulative_price_volume = 0.0;
    double cumulative_volume = 0.0;
    std::optional<double> previous_close;
    std::optional<double> previous_vwap;
  };

  struct _candle_snapshot {
    bool latest_candle_included = false;
    double latest_close = 0.0;
    std::optional<double> prior_close;
    std::optional<double> prior_vwap;
    std::optional<double> latest_vwap;
  };

  _candle_snapshot
  _process_new_candles(const aggregations& market, _session_state& state) const;

  decision _evaluate_signal(
      stock::Symbol symbol,
      const trading_state& data,
      const _candle_snapshot& snapshot) const;

  options _options;
  std::unordered_map<stock::Symbol, _session_state> _states;
};

} // namespace howling
