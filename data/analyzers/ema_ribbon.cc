#include "data/analyzers/ema_ribbon.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer_utils.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

ema_ribbon_analyzer::_ema_tracker::_ema_tracker(int period) : _period(period) {
  if (_period <= 0) {
    throw std::invalid_argument("EMA period must be positive.");
  }
}

bool ema_ribbon_analyzer::_ema_tracker::update(double sample) {
  if (++_sample_count <= _period) {
    _seed_sum += sample;
    if (_sample_count == _period) {
      _value = _seed_sum / static_cast<double>(_period);
    }
    return has_value();
  }

  const double multiplier = 2.0 / (static_cast<double>(_period) + 1.0);
  *_value = (sample * multiplier) + (*_value * (1.0 - multiplier));
  return true;
}

void ema_ribbon_analyzer::_ema_tracker::reset() {
  _sample_count = 0;
  _seed_sum = 0.0;
  _value.reset();
}

ema_ribbon_analyzer::ema_ribbon_analyzer(
    vector<window> aggregations::* period, options opts)
    : _options(std::move(opts)), _period(period),
      _expected_window_size(expected_window_size_for(period)) {
  if (_options.periods.size() < 2) {
    throw std::invalid_argument("EMA ribbon requires at least two periods.");
  }
  for (int period_length : _options.periods) {
    if (period_length <= 0) {
      throw std::invalid_argument("EMA ribbon periods must be positive.");
    }
  }
  std::sort(_options.periods.begin(), _options.periods.end());
  auto duplicate =
      std::adjacent_find(_options.periods.begin(), _options.periods.end());
  if (duplicate != _options.periods.end()) {
    throw std::invalid_argument(
        std::format(
            "EMA ribbon periods must be unique; duplicate period {}.",
            *duplicate));
  }
  if (!std::isfinite(_options.min_spread_ratio) ||
      _options.min_spread_ratio < 0.0) {
    throw std::invalid_argument(
        "Minimum spread ratio must be finite and non-negative.");
  }
  if (!std::isfinite(_options.confidence_scaler) ||
      _options.confidence_scaler <= 0.0) {
    throw std::invalid_argument(
        "Confidence scaler must be finite and positive.");
  }
}

decision
ema_ribbon_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  const aggregations* symbol_aggregation = find_aggregations(symbol, data);
  if (symbol_aggregation == nullptr) return NO_ACTION;

  const vector<window>& series = symbol_aggregation->*_period;
  if (series.empty()) return NO_ACTION;

  const int window_size = window_size_for(series);
  if (window_size < _expected_window_size) return NO_ACTION;

  if (!is_window_step_boundary(data, std::chrono::minutes{window_size})) {
    return NO_ACTION;
  }

  _symbol_state& state = _get_or_create_state(symbol);
  _update_trackers(state, series, window_size);

  if (!state.trackers.back().has_value()) {
    state.last_trend = trend::UNKNOWN;
    return NO_ACTION;
  }

  const double spread = _compute_spread(state);
  const trend ribbon_trend = _classify_trend(state, spread);
  state.last_trend = ribbon_trend;
  return _build_decision(
      ribbon_trend, spread, symbol, *symbol_aggregation, data);
}

ema_ribbon_analyzer::trend
ema_ribbon_analyzer::current_trend(stock::Symbol symbol) const {
  auto itr = _states.find(symbol);
  if (itr == _states.end()) return trend::UNKNOWN;
  return itr->second.last_trend;
}

std::optional<std::vector<double>>
ema_ribbon_analyzer::current_emas(stock::Symbol symbol) const {
  auto itr = _states.find(symbol);
  if (itr == _states.end()) return std::nullopt;
  const _symbol_state& state = itr->second;
  if (state.trackers.empty() || !state.trackers.back().has_value()) {
    return std::nullopt;
  }

  std::vector<double> values;
  values.reserve(state.trackers.size());
  for (const _ema_tracker& tracker : state.trackers) {
    values.push_back(*tracker.value());
  }
  return values;
}

ema_ribbon_analyzer::_symbol_state&
ema_ribbon_analyzer::_get_or_create_state(stock::Symbol symbol) {
  auto [itr, inserted] = _states.try_emplace(symbol);
  if (inserted) {
    itr->second.trackers.reserve(_options.periods.size());
    for (int period_length : _options.periods) {
      itr->second.trackers.emplace_back(period_length);
    }
  }
  return itr->second;
}

void ema_ribbon_analyzer::_reset_state(_symbol_state& state) const {
  state.last_processed_count = 0;
  state.last_trend = trend::UNKNOWN;
  for (_ema_tracker& tracker : state.trackers) tracker.reset();
}

void ema_ribbon_analyzer::_update_trackers(
    _symbol_state& state, const vector<window>& series, int window_size) const {
  if (series.size() < state.last_processed_count) _reset_state(state);
  if (series.size() <= state.last_processed_count) return;

  // Step through the `delta` unprocessed entries using stride `step` (the
  // window size in minutes). `start_index` aligns the stride so the final
  // iteration always lands on the latest completed bar at `series.size() - 1`.
  const size_t step = static_cast<size_t>(window_size);
  const size_t delta = series.size() - state.last_processed_count;
  size_t start_index = series.size() - 1;
  if (delta >= step) {
    const size_t remainder = (delta - 1) % step;
    start_index = state.last_processed_count + remainder;
  }
  for (size_t i = start_index; i < series.size(); i += step) {
    const double close_price = series[i].candle.close();
    for (_ema_tracker& tracker : state.trackers) tracker.update(close_price);
  }
  state.last_processed_count = series.size();
}

double ema_ribbon_analyzer::_compute_spread(const _symbol_state& state) const {
  const double ema_fast = *state.trackers.front().value();
  const double ema_slow = *state.trackers.back().value();
  const double slow_magnitude = std::abs(ema_slow);
  if (slow_magnitude == 0.0) return 0.0;
  return std::abs(ema_fast - ema_slow) / slow_magnitude;
}

ema_ribbon_analyzer::trend ema_ribbon_analyzer::_classify_trend(
    const _symbol_state& state, double spread) const {
  const double slow_magnitude = std::abs(*state.trackers.back().value());
  if (slow_magnitude == 0.0 || spread < _options.min_spread_ratio) {
    return trend::TANGLED;
  }

  bool is_bullish = true;
  bool is_bearish = true;
  for (size_t i = 0; i + 1 < state.trackers.size(); ++i) {
    const double faster_ema = *state.trackers[i].value();
    const double slower_ema = *state.trackers[i + 1].value();
    if (faster_ema <= slower_ema) is_bullish = false;
    if (faster_ema >= slower_ema) is_bearish = false;
  }

  if (is_bullish) return trend::BULLISH;
  if (is_bearish) return trend::BEARISH;
  return trend::TANGLED;
}

decision ema_ribbon_analyzer::_build_decision(
    trend ribbon_trend,
    double spread,
    stock::Symbol symbol,
    const aggregations& symbol_aggregation,
    const trading_state& data) const {
  if (ribbon_trend == trend::BULLISH) {
    const double confidence =
        std::clamp(spread * _options.confidence_scaler, 0.0, 1.0);
    const bool buy_allowed =
        !symbol_aggregation.one_minute.empty() && can_buy(symbol, data);
    return {
        .act = buy_allowed ? action::BUY : action::HOLD,
        .confidence = confidence,
    };
  }

  if (ribbon_trend == trend::BEARISH) {
    const double confidence =
        std::clamp(spread * _options.confidence_scaler, 0.0, 1.0);
    return {
        .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
        .confidence = confidence,
    };
  }

  const double spread_denominator = std::max(_options.min_spread_ratio, 1e-9);
  const double confidence =
      1.0 - std::clamp(spread / spread_denominator, 0.0, 1.0);
  return {
      .act = action::HOLD,
      .confidence = confidence,
  };
}

} // namespace howling
