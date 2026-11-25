#include "data/analyzers/vwap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer_utils.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "time/conversion.h"
#include "time/market_time.h"
#include "trading/trading_state.h"

namespace howling {
namespace {

constexpr double MIN_THRESHOLD_SCALE = 1e-6;
constexpr double MIN_CONFIDENCE = 0.1;
constexpr double MAX_CONFIDENCE = 1.0;

} // namespace

vwap_analyzer::vwap_analyzer(options opts) : _options(opts) {
  if (_options.min_candles <= 0) {
    throw std::invalid_argument("min_candles must be positive.");
  }
  if (!std::isfinite(_options.deviation_threshold) ||
      _options.deviation_threshold < 0.0) {
    throw std::invalid_argument(
        "deviation_threshold must be finite and non-negative.");
  }
}

decision
vwap_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  const aggregations* market = find_aggregations(symbol, data);
  if (market == nullptr || market->one_minute.empty()) return NO_ACTION;

  _session_state& state = _states[symbol];
  const std::size_t total_candles = market->one_minute.size();
  if (total_candles < state.last_processed_count) {
    // The candle history shrank or was replaced; reset all tracked state.
    state = _session_state{};
  }
  if (state.last_processed_count == total_candles) return NO_ACTION;

  const _candle_snapshot snapshot = _process_new_candles(*market, state);
  state.last_processed_count = total_candles;

  if (!snapshot.latest_candle_included || !snapshot.latest_vwap.has_value()) {
    return NO_ACTION;
  }
  if (state.session_candle_count < _options.min_candles) return NO_ACTION;
  if (*snapshot.latest_vwap <= 0.0) return NO_ACTION;

  return _evaluate_signal(symbol, data, snapshot);
}

vwap_analyzer::_candle_snapshot vwap_analyzer::_process_new_candles(
    const aggregations& market, _session_state& state) const {
  _candle_snapshot snapshot;
  const std::size_t total_candles = market.one_minute.size();

  for (std::size_t i = state.last_processed_count; i < total_candles; ++i) {
    const Candle& candle = market.one_minute[i].candle;
    const std::chrono::system_clock::time_point opened_at =
        to_std_chrono(candle.opened_at());
    const std::chrono::local_days trading_day = market_trading_day(opened_at);

    if (!state.session_day.has_value() || trading_day != *state.session_day) {
      state.session_day = trading_day;
      state.cumulative_price_volume = 0.0;
      state.cumulative_volume = 0.0;
      state.session_candle_count = 0;
      state.previous_close = std::nullopt;
      state.previous_vwap = std::nullopt;
    }

    if (!_options.include_extended_hours &&
        !is_regular_market_hours(opened_at)) {
      snapshot.latest_candle_included = false;
      continue;
    }

    const double typical_price =
        (candle.high() + candle.low() + candle.close()) / 3.0;
    if (candle.volume() > 0) {
      const double volume = static_cast<double>(candle.volume());
      state.cumulative_price_volume += typical_price * volume;
      state.cumulative_volume += volume;
    }
    ++state.session_candle_count;

    // Capture the previous bar's close and VWAP before updating state for this
    // bar so crossover checks compare consecutive session bars.
    snapshot.prior_close = state.previous_close;
    snapshot.prior_vwap = state.previous_vwap;
    snapshot.latest_close = candle.close();
    snapshot.latest_candle_included = true;

    if (state.cumulative_volume > 0.0) {
      const double vwap =
          state.cumulative_price_volume / state.cumulative_volume;
      snapshot.latest_vwap = vwap;
      state.previous_close = snapshot.latest_close;
      state.previous_vwap = vwap;
    } else {
      snapshot.latest_vwap = std::nullopt;
    }
  }

  return snapshot;
}

decision vwap_analyzer::_evaluate_signal(
    stock::Symbol symbol,
    const trading_state& data,
    const _candle_snapshot& snapshot) const {
  const double vwap = *snapshot.latest_vwap;
  const double relative_deviation = (snapshot.latest_close - vwap) / vwap;
  const double threshold_scale =
      std::max(_options.deviation_threshold, MIN_THRESHOLD_SCALE);

  if (_options.strategy_mode == mode::TREND_FILTER) {
    if (!snapshot.prior_close.has_value() || !snapshot.prior_vwap.has_value()) {
      return NO_ACTION;
    }
    if (*snapshot.prior_close <= *snapshot.prior_vwap &&
        snapshot.latest_close > vwap && can_buy(symbol, data)) {
      const double confidence = std::clamp(
          relative_deviation / threshold_scale, MIN_CONFIDENCE, MAX_CONFIDENCE);
      return {.act = action::BUY, .confidence = confidence};
    }
    if (*snapshot.prior_close >= *snapshot.prior_vwap &&
        snapshot.latest_close < vwap && can_sell(symbol, data)) {
      const double confidence = std::clamp(
          -relative_deviation / threshold_scale,
          MIN_CONFIDENCE,
          MAX_CONFIDENCE);
      return {.act = action::SELL, .confidence = confidence};
    }
    return NO_ACTION;
  }

  if (relative_deviation <= -_options.deviation_threshold &&
      can_buy(symbol, data)) {
    const double confidence = std::clamp(
        -relative_deviation / threshold_scale, MIN_CONFIDENCE, MAX_CONFIDENCE);
    return {.act = action::BUY, .confidence = confidence};
  }
  if (relative_deviation >= _options.deviation_threshold &&
      can_sell(symbol, data)) {
    const double confidence = std::clamp(
        relative_deviation / threshold_scale, MIN_CONFIDENCE, MAX_CONFIDENCE);
    return {.act = action::SELL, .confidence = confidence};
  }

  return NO_ACTION;
}

std::optional<double> vwap_analyzer::current_vwap(stock::Symbol symbol) const {
  auto itr = _states.find(symbol);
  if (itr == _states.end() || itr->second.cumulative_volume <= 0.0) {
    return std::nullopt;
  }
  return itr->second.cumulative_price_volume / itr->second.cumulative_volume;
}

} // namespace howling
