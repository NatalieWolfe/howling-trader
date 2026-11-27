#include "data/analyzers/rsi.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <stdexcept>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer_utils.h"
#include "data/stock.pb.h"
#include "data/wilder_rma.h"
#include "trading/trading_state.h"

namespace howling {
namespace {

double compute_rsi(double average_gain, double average_loss) {
  if (average_loss == 0.0 && average_gain == 0.0) return 50.0;
  if (average_loss == 0.0) return 100.0;
  if (average_gain == 0.0) return 0.0;
  const double relative_strength = average_gain / average_loss;
  return 100.0 - (100.0 / (1.0 + relative_strength));
}

} // namespace

rsi_analyzer::rsi_analyzer(
    vector<window> aggregations::* candle_period, options opts)
    : _options(opts), _candle_period(candle_period),
      _minimum_window_size(expected_window_size_for(candle_period)) {
  if (_options.period <= 0) {
    throw std::invalid_argument("RSI period must be positive.");
  }
  if (!(0.0 < _options.oversold_threshold &&
        _options.oversold_threshold < _options.overbought_threshold &&
        _options.overbought_threshold < 100.0)) {
    throw std::invalid_argument(
        "RSI thresholds must satisfy 0 < oversold < overbought < 100.");
  }
}

void rsi_analyzer::_update_state(
    _symbol_state& state, const vector<window>& series) {
  if (series.size() < state.last_processed_count) {
    state.last_processed_count = 0;
    state.previous_close.reset();
    state.average_gain.reset();
    state.average_loss.reset();
    state.last_rsi.reset();
  }

  if (series.size() <= state.last_processed_count) return;

  const double current_close = series(-1).candle.close();
  if (state.previous_close.has_value()) {
    const double delta = current_close - *state.previous_close;
    const double gain = std::max(0.0, delta);
    const double loss = std::max(0.0, -delta);
    const bool gain_ready = state.average_gain.update(gain);
    const bool loss_ready = state.average_loss.update(loss);
    if (gain_ready && loss_ready) {
      state.last_rsi =
          compute_rsi(*state.average_gain.value(), *state.average_loss.value());
    }
  }
  state.previous_close = current_close;
  state.last_processed_count = series.size();
}

decision
rsi_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  const aggregations* symbol_aggregation = find_aggregations(symbol, data);
  if (symbol_aggregation == nullptr) return NO_ACTION;

  const vector<window>& series = symbol_aggregation->*_candle_period;
  if (series.empty()) return NO_ACTION;

  const int window_size = window_size_for(series);
  if (window_size < _minimum_window_size ||
      !is_window_step_boundary(data, std::chrono::minutes{window_size})) {
    return NO_ACTION;
  }

  auto [itr, inserted] = _symbol_states.try_emplace(symbol, _options.period);
  _symbol_state& state = itr->second;
  _update_state(state, series);

  if (!state.last_rsi.has_value()) return NO_ACTION;

  const double rsi = *state.last_rsi;
  if (rsi < _options.oversold_threshold) {
    const double confidence = std::clamp(
        (_options.oversold_threshold - rsi) / _options.oversold_threshold,
        0.0,
        1.0);
    return {
        .act = can_buy(symbol, data) ? action::BUY : action::HOLD,
        .confidence = confidence,
    };
  }
  if (rsi > _options.overbought_threshold) {
    const double confidence = std::clamp(
        (rsi - _options.overbought_threshold) /
            (100.0 - _options.overbought_threshold),
        0.0,
        1.0);
    return {
        .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
        .confidence = confidence,
    };
  }

  return NO_ACTION;
}

std::optional<double> rsi_analyzer::current_rsi(stock::Symbol symbol) const {
  auto itr = _symbol_states.find(symbol);
  if (itr == _symbol_states.end()) return std::nullopt;
  return itr->second.last_rsi;
}

} // namespace howling
