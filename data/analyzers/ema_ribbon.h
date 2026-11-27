#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * @brief Configuration parameters for the EMA ribbon analyzer.
 */
struct ema_ribbon_options {
  std::vector<int> periods = {9, 21, 50};
  double min_spread_ratio = 0.001;
  // Scales relative ribbon spread into signal confidence in [0.0, 1.0].
  // Defaulting to 100.0 reaches full confidence around a ~1% intraday EMA
  // separation typical of a strong equity trend while damping choppy noise;
  // tune higher for lower-volatility stocks or lower for volatile ones.
  double confidence_scaler = 100.0;
};

/**
 * @brief Analyzes trend direction and strength using an Exponential Moving
 * Average (EMA) ribbon across multiple configurable periods.
 *
 * Thread-compatible: external synchronization is required for concurrent
 * access.
 */
class ema_ribbon_analyzer : public analyzer {
public:
  /**
   * @brief Current alignment state of the EMA ribbon for a symbol.
   */
  enum class trend { UNKNOWN = 0, BULLISH = 1, BEARISH = 2, TANGLED = 3 };

  using options = ema_ribbon_options;

  /**
   * @brief Constructs an EMA ribbon analyzer with the specified aggregation
   * period and options.
   *
   * @param period
   *  Member pointer selecting the candle aggregation series.
   * @param opts
   *  Ribbon periods (sorted ascending internally), minimum spread ratio, and
   *  confidence scaling factor.
   * @throws std::invalid_argument
   *  If period is null or not a recognized aggregations window series, fewer
   *  than 2 periods are provided, any period is non-positive, duplicate periods
   *  exist, min_spread_ratio is not finite or negative, or confidence_scaler is
   *  not finite or non-positive.
   */
  explicit ema_ribbon_analyzer(
      vector<window> aggregations::* period = &aggregations::one_minute,
      options opts = {});

  /**
   * @brief Evaluates the EMA ribbon for the given symbol and trading state.
   *
   * @param symbol Stock symbol to analyze.
   * @param data Current market and account trading state.
   * @return Trading decision (BUY, SELL, HOLD, or NO_ACTION).
   */
  decision analyze(stock::Symbol symbol, const trading_state& data) override;

  /**
   * @brief Returns the latest classified ribbon trend for the given symbol.
   *
   * @param symbol Stock symbol to query.
   * @return Latest trend classification, or trend::UNKNOWN if warmed-up state
   * is not yet available.
   */
  [[nodiscard]] trend current_trend(stock::Symbol symbol) const;

  /**
   * @brief Returns the current EMA values ordered from fastest to slowest
   * period once all ribbon EMAs have warmed up.
   *
   * @param symbol Stock symbol to query.
   * @return Vector of EMA values, or std::nullopt if the symbol has not been
   * seen or is still warming up.
   */
  [[nodiscard]] std::optional<std::vector<double>>
  current_emas(stock::Symbol symbol) const;

private:
  /**
   * @brief Exponential Moving Average (EMA) tracker, seeded with a simple
   * moving average over the first period samples.
   */
  class _ema_tracker {
  public:
    explicit _ema_tracker(int period);

    bool update(double sample);
    [[nodiscard]] bool has_value() const { return _value.has_value(); }
    [[nodiscard]] std::optional<double> value() const { return _value; }
    [[nodiscard]] int period() const { return _period; }
    void reset();

  private:
    int _period;
    int _sample_count = 0;
    double _seed_sum = 0.0;
    std::optional<double> _value;
  };

  struct _symbol_state {
    size_t last_processed_count = 0;
    std::vector<_ema_tracker> trackers;
    trend last_trend = trend::UNKNOWN;
  };

  _symbol_state& _get_or_create_state(stock::Symbol symbol);
  void _reset_state(_symbol_state& state) const;
  void _update_trackers(
      _symbol_state& state,
      const vector<window>& series,
      int window_size) const;
  [[nodiscard]] double _compute_spread(const _symbol_state& state) const;
  [[nodiscard]] trend
  _classify_trend(const _symbol_state& state, double spread) const;
  [[nodiscard]] decision _build_decision(
      trend ribbon_trend,
      double spread,
      stock::Symbol symbol,
      const aggregations& symbol_aggregation,
      const trading_state& data) const;

  options _options;
  vector<window> aggregations::* _period;
  int _expected_window_size;
  std::unordered_map<stock::Symbol, _symbol_state> _states;
};

} // namespace howling
