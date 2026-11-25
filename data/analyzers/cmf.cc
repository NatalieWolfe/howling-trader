#include "data/analyzers/cmf.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer_utils.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {
namespace {

constexpr double DIVERGENCE_EPSILON = 1e-9;

std::pair<double, double> resolve_bar_high_low(
    const aggregations& symbol_aggregation,
    const Candle& candle,
    int index,
    int window_size,
    size_t series_size) {
  double high = candle.high();
  double low = candle.low();
  // Multi-minute aggregations from do_aggregate() start from blank_window()
  // with low == 0.0. Recover the true high/low from constituent 1-minute bars
  // when available.
  if (window_size > 1 && low == 0.0 &&
      symbol_aggregation.one_minute.size() == series_size) {
    high = symbol_aggregation.one_minute(index).candle.high();
    low = symbol_aggregation.one_minute(index).candle.low();
    for (int j = 1; j < window_size; ++j) {
      const Candle& minute_candle =
          symbol_aggregation.one_minute(index - j).candle;
      high = std::max(high, minute_candle.high());
      low = std::min(low, minute_candle.low());
    }
  }
  return {high, low};
}

double find_prior_max_high(
    const vector<window>& series, int high_check_bars, int window_size) {
  double prior_max_high = series(-1 - window_size).candle.high();
  for (int offset = 1; offset <= high_check_bars; ++offset) {
    const double bar_high = series(-1 - (offset * window_size)).candle.high();
    if (bar_high > prior_max_high) prior_max_high = bar_high;
  }
  return prior_max_high;
}

} // namespace

cmf_analyzer::cmf_analyzer(vector<window> aggregations::* period, options opts)
    : _period{period}, _options{opts} {
  if (_period == nullptr) {
    throw std::invalid_argument("CMF period member pointer must not be null.");
  }
  if (_options.lookback_periods <= 0) {
    throw std::invalid_argument("CMF lookback_periods must be positive.");
  }
  if (!(_options.threshold >= 0.0 && _options.threshold < 1.0)) {
    throw std::invalid_argument("CMF threshold must be in [0.0, 1.0).");
  }
  if (!std::isfinite(_options.confidence_scaler) ||
      _options.confidence_scaler <= 0.0) {
    throw std::invalid_argument(
        "CMF confidence_scaler must be positive and finite.");
  }
}

std::optional<double> cmf_analyzer::calculate_cmf(
    stock::Symbol symbol, const trading_state& data) const {
  const aggregations* symbol_aggregation = find_aggregations(symbol, data);
  if (symbol_aggregation == nullptr) return std::nullopt;

  const vector<window>& series = symbol_aggregation->*_period;
  const int window_size = window_size_for(series);
  return _calculate_cmf_at(*symbol_aggregation, /*offset_bars=*/0, window_size);
}

std::optional<double> cmf_analyzer::_calculate_cmf_at(
    const aggregations& symbol_aggregation,
    int offset_bars,
    int window_size) const {
  if (window_size <= 0 || offset_bars < 0) return std::nullopt;

  const vector<window>& series = symbol_aggregation.*_period;
  const size_t required_size =
      static_cast<size_t>(_options.lookback_periods + offset_bars) *
      static_cast<size_t>(window_size);
  if (series.size() < required_size) return std::nullopt;

  double money_flow_volume_sum = 0.0;
  double volume_sum = 0.0;
  for (int i = 0; i < _options.lookback_periods; ++i) {
    const int index = -1 - ((offset_bars + i) * window_size);
    const Candle& candle = series(index).candle;
    const double volume = static_cast<double>(candle.volume());
    if (volume <= 0.0) continue;

    const std::pair<double, double> high_low = resolve_bar_high_low(
        symbol_aggregation, candle, index, window_size, series.size());
    const double range = high_low.first - high_low.second;
    double money_flow_multiplier = 0.0;
    if (range > 0.0) {
      money_flow_multiplier = ((candle.close() - high_low.second) -
                               (high_low.first - candle.close())) /
          range;
    }
    money_flow_volume_sum += money_flow_multiplier * volume;
    volume_sum += volume;
  }

  if (volume_sum <= 0.0) return std::nullopt;
  return std::clamp(money_flow_volume_sum / volume_sum, -1.0, 1.0);
}

std::optional<double> cmf_analyzer::_detect_bearish_divergence(
    const aggregations& symbol_aggregation,
    double current_cmf,
    int window_size) const {
  if (!_options.enable_divergence_check || window_size <= 0) {
    return std::nullopt;
  }

  const vector<window>& series = symbol_aggregation.*_period;
  const int total_bars = static_cast<int>(series.size() / window_size);
  const int available_prior_bars = total_bars - _options.lookback_periods;
  if (available_prior_bars < 1) return std::nullopt;

  const std::optional<double> previous_cmf =
      _calculate_cmf_at(symbol_aggregation, /*offset_bars=*/1, window_size);
  if (!previous_cmf.has_value()) return std::nullopt;

  // CMF must be trending downward relative to the immediately preceding bar.
  if (current_cmf + DIVERGENCE_EPSILON >= *previous_cmf) {
    return std::nullopt;
  }

  const int cmf_check_bars =
      std::min(available_prior_bars, _options.lookback_periods);
  const int high_check_bars =
      std::max(cmf_check_bars, _options.lookback_periods - 1);
  if (series(-1).candle.high() <=
      find_prior_max_high(series, high_check_bars, window_size)) {
    return std::nullopt;
  }

  double prior_max_cmf = *previous_cmf;
  for (int offset = 2; offset <= cmf_check_bars; ++offset) {
    const std::optional<double> prior_cmf =
        _calculate_cmf_at(symbol_aggregation, offset, window_size);
    if (prior_cmf.has_value() && *prior_cmf > prior_max_cmf) {
      prior_max_cmf = *prior_cmf;
    }
  }

  if (current_cmf + DIVERGENCE_EPSILON < prior_max_cmf) {
    return prior_max_cmf - current_cmf;
  }
  return std::nullopt;
}

decision
cmf_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  const aggregations* symbol_aggregation = find_aggregations(symbol, data);
  if (symbol_aggregation == nullptr) return NO_ACTION;

  const vector<window>& series = symbol_aggregation->*_period;
  const int window_size = window_size_for(series);
  const std::optional<double> cmf_value =
      _calculate_cmf_at(*symbol_aggregation, /*offset_bars=*/0, window_size);
  if (!cmf_value.has_value()) return NO_ACTION;

  const std::optional<double> bearish_divergence_drop =
      _detect_bearish_divergence(*symbol_aggregation, *cmf_value, window_size);
  if (bearish_divergence_drop.has_value()) {
    const double signal_strength =
        std::max(*bearish_divergence_drop, -(*cmf_value));
    return {
        .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
        .confidence =
            std::clamp(signal_strength * _options.confidence_scaler, 0.0, 1.0)};
  }

  if (*cmf_value > _options.threshold) {
    return {
        .act = can_buy(symbol, data) ? action::BUY : action::HOLD,
        .confidence = std::min(1.0, (*cmf_value) * _options.confidence_scaler)};
  }

  if (*cmf_value < -_options.threshold) {
    return {
        .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
        .confidence =
            std::min(1.0, -(*cmf_value) * _options.confidence_scaler)};
  }

  return NO_ACTION;
}

} // namespace howling
