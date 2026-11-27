#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "containers/circular_buffer.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * @brief Configuration options for the gap and momentum analyzer.
 */
struct gap_momentum_options {
  // Minimum fractional overnight gap up (e.g., 0.02 for 2%) required to emit a
  // Gap and Go BUY breakout signal. Also scales the gap's confidence weight.
  double momentum_gap_threshold = 0.02;
  // Minimum fractional overnight gap up (e.g., 0.03 for 3%) required to emit a
  // low-volume Gap Fill mean-reversion signal. Also scales signal confidence.
  double fill_gap_threshold = 0.03;
  // Minimum ratio of today's opening-window volume to the historical baseline
  // required to confirm a high-volume Gap and Go breakout.
  double high_relative_volume = 1.5;
  // Maximum ratio of today's opening-window volume to the historical baseline
  // below which a large gap up is treated as a weak-volume Gap Fill setup.
  double low_relative_volume = 0.8;
  // Length in minutes after regular market open used to establish the opening
  // range and opening volume. Signals are suppressed until this window closes.
  int opening_window_minutes = 5;
  // Maximum minutes after regular market open during which gap signals may
  // fire. Must be at least opening_window_minutes.
  int active_window_minutes = 90;
  // Number of prior trading sessions' opening-window volumes averaged to form
  // the baseline for relative volume calculations.
  int volume_baseline_days = 10;
};

/**
 * @brief Analyzes overnight price gaps and early-session relative volume to
 * identify Gap and Go momentum breakouts and low-volume Gap Fill mean
 * reversion setups.
 *
 * Thread-compatible: external synchronization is required for concurrent
 * access.
 */
class gap_momentum_analyzer : public analyzer {
public:
  using options = gap_momentum_options;

  /**
   * @brief Constructs a gap and momentum analyzer with the specified options.
   *
   * @param opts Configuration thresholds and window lengths.
   * @throws std::invalid_argument if any threshold or window is non-positive,
   * or if active_window_minutes is less than opening_window_minutes.
   */
  explicit gap_momentum_analyzer(options opts = {});

  /**
   * @brief Evaluates gap and early-session volume conditions for a symbol.
   *
   * @param symbol Stock symbol to analyze.
   * @param data Current trading state and market aggregations.
   * @return Trading decision with confidence in [0.0, 1.0].
   */
  decision analyze(stock::Symbol symbol, const trading_state& data) override;

private:
  struct _symbol_session_state {
    explicit _symbol_session_state(uint32_t volume_baseline_days)
        : prior_opening_volumes(volume_baseline_days) {}

    circular_buffer<double> prior_opening_volumes;
    std::optional<std::chrono::local_days> current_session_day;
    double today_open = 0.0;
    double opening_window_volume = 0.0;
    double opening_range_high = 0.0;
    double opening_range_low = 0.0;
    bool has_opening_window_data = false;
    std::size_t processed_candles = 0;
  };

  static void _start_new_session(
      _symbol_session_state& state,
      std::chrono::local_days trading_day,
      double open_price);

  void _update_state(
      _symbol_session_state& state,
      const aggregations& symbol_aggregations) const;

  decision _evaluate_gap_signal(
      stock::Symbol symbol,
      const trading_state& data,
      const _symbol_session_state& state,
      double gap_percent,
      double relative_volume,
      double latest_close) const;

  options _options;
  std::unordered_map<stock::Symbol, _symbol_session_state> _sessions;
};

} // namespace howling
