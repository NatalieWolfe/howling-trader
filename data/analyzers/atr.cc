#include "data/analyzers/atr.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer_utils.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "data/wilder_rma.h"
#include "trading/trading_state.h"

namespace howling {
namespace {

double compute_true_range(
    const aggregations& symbol_aggregation,
    const Candle& candle,
    int window_size,
    std::optional<double> previous_close) {
  double high = candle.high();
  double low = candle.low();
  if (window_size > 1 && low == 0.0 &&
      symbol_aggregation.one_minute.size() >=
          static_cast<size_t>(window_size)) {
    // Multi-minute windows from do_aggregate() start with a blank candle whose
    // low is 0.0, so derive the true high/low from the constituent 1m bars.
    high = symbol_aggregation.one_minute(-1).candle.high();
    low = symbol_aggregation.one_minute(-1).candle.low();
    for (int i = 2; i <= window_size; ++i) {
      const Candle& sub_candle = symbol_aggregation.one_minute(-i).candle;
      high = std::max(high, sub_candle.high());
      low = std::min(low, sub_candle.low());
    }
  }
  const double high_low_delta = high - low;
  if (!previous_close.has_value()) return high_low_delta;
  return std::max({
      high_low_delta,
      std::abs(high - *previous_close),
      std::abs(low - *previous_close),
  });
}

} // namespace

atr_analyzer::_symbol_state::_symbol_state(int period, int baseline_period)
    : rma(period), baseline(static_cast<uint32_t>(baseline_period)) {}

atr_analyzer::atr_analyzer(vector<window> aggregations::* period, options opts)
    : _options(opts), _period(period),
      _expected_window_size(expected_window_size_for(period)) {
  if (_options.period <= 0) {
    throw std::invalid_argument("ATR period must be positive.");
  }
  if (_options.baseline_period <= 0) {
    throw std::invalid_argument("ATR baseline_period must be positive.");
  }
  if (!(_options.high_volatility_ratio > 0.0) ||
      !std::isfinite(_options.high_volatility_ratio)) {
    throw std::invalid_argument(
        "ATR high_volatility_ratio must be positive and finite.");
  }
  if (_period == nullptr) {
    throw std::invalid_argument("ATR period member pointer must not be null.");
  }
}

decision
atr_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  const aggregations* symbol_aggregation = find_aggregations(symbol, data);
  if (symbol_aggregation == nullptr) return NO_ACTION;

  const vector<window>& series = symbol_aggregation->*_period;
  if (series.empty()) return NO_ACTION;

  const int window_size = window_size_for(series);
  if (window_size < _expected_window_size ||
      !is_window_step_boundary(data, std::chrono::minutes{window_size})) {
    return NO_ACTION;
  }

  auto [itr, inserted] =
      _states.try_emplace(symbol, _options.period, _options.baseline_period);
  _symbol_state& state = itr->second;

  std::optional<double> previous_close = state.previous_close;
  if (!previous_close.has_value() &&
      series.size() >= static_cast<size_t>(window_size) + 1) {
    previous_close = series(-window_size - 1).candle.close();
  }

  const Candle& candle = series(-1).candle;
  const double true_range = compute_true_range(
      *symbol_aggregation, candle, window_size, previous_close);
  state.previous_close = candle.close();

  if (!state.rma.update(true_range)) return NO_ACTION;

  const double atr_value = *state.rma.value();
  if (state.baseline.size() == state.baseline.capacity()) {
    state.baseline_sum =
        std::max(0.0, state.baseline_sum - state.baseline.front());
  }
  state.baseline.push_back(atr_value);
  state.baseline_sum += atr_value;

  const double historical_average_atr =
      state.baseline_sum / static_cast<double>(state.baseline.size());
  if (historical_average_atr <= 0.0) {
    return {.act = action::NO_ACTION, .confidence = 1.0};
  }

  const double ratio = atr_value / historical_average_atr;
  if (ratio >= _options.high_volatility_ratio) {
    const double hold_confidence =
        std::clamp((ratio - 1.0) / _options.high_volatility_ratio, 0.0, 1.0);
    return {.act = action::HOLD, .confidence = hold_confidence};
  }

  const double confidence = std::clamp(1.0 - ratio, 0.0, 1.0);
  return {.act = action::NO_ACTION, .confidence = confidence};
}

std::optional<double> atr_analyzer::current_atr(stock::Symbol symbol) const {
  auto itr = _states.find(symbol);
  if (itr == _states.end()) return std::nullopt;
  return itr->second.rma.value();
}

std::optional<double> atr_analyzer::stop_loss_distance(
    stock::Symbol symbol, double multiplier) const {
  if (!(multiplier > 0.0) || !std::isfinite(multiplier)) {
    throw std::invalid_argument(
        "Stop-loss multiplier must be positive and finite.");
  }
  const std::optional<double> atr = current_atr(symbol);
  if (!atr.has_value()) return std::nullopt;
  return *atr * multiplier;
}

} // namespace howling
