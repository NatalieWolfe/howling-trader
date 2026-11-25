#include "data/analyzers/gap_momentum.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

bool is_positive_finite(double value) {
  return std::isfinite(value) && value > 0.0;
}

void validate_options(const gap_momentum_analyzer::options& analyzer_options) {
  if (!is_positive_finite(analyzer_options.momentum_gap_threshold)) {
    throw std::invalid_argument(
        "momentum_gap_threshold must be positive and finite.");
  }
  if (!is_positive_finite(analyzer_options.fill_gap_threshold)) {
    throw std::invalid_argument(
        "fill_gap_threshold must be positive and finite.");
  }
  if (!is_positive_finite(analyzer_options.high_relative_volume)) {
    throw std::invalid_argument(
        "high_relative_volume must be positive and finite.");
  }
  if (!is_positive_finite(analyzer_options.low_relative_volume)) {
    throw std::invalid_argument(
        "low_relative_volume must be positive and finite.");
  }
  if (analyzer_options.opening_window_minutes <= 0) {
    throw std::invalid_argument("opening_window_minutes must be positive.");
  }
  if (analyzer_options.active_window_minutes <= 0) {
    throw std::invalid_argument("active_window_minutes must be positive.");
  }
  if (analyzer_options.active_window_minutes <
      analyzer_options.opening_window_minutes) {
    throw std::invalid_argument(
        "active_window_minutes must be at least opening_window_minutes.");
  }
  if (analyzer_options.volume_baseline_days <= 0) {
    throw std::invalid_argument("volume_baseline_days must be positive.");
  }
}

} // namespace

gap_momentum_analyzer::gap_momentum_analyzer(options opts) : _options(opts) {
  validate_options(_options);
}

void gap_momentum_analyzer::_start_new_session(
    _symbol_session_state& state,
    std::chrono::local_days trading_day,
    double open_price) {
  state.current_session_day = trading_day;
  state.today_open = open_price;
  state.opening_window_volume = 0.0;
  state.opening_range_high = 0.0;
  state.opening_range_low = 0.0;
  state.has_opening_window_data = false;
}

void gap_momentum_analyzer::_update_state(
    _symbol_session_state& state,
    const aggregations& symbol_aggregations) const {
  if (symbol_aggregations.one_minute.size() < state.processed_candles) {
    state.prior_opening_volumes.clear();
    state.current_session_day.reset();
    state.today_open = 0.0;
    state.opening_window_volume = 0.0;
    state.opening_range_high = 0.0;
    state.opening_range_low = 0.0;
    state.has_opening_window_data = false;
    state.processed_candles = 0;
  }

  for (std::size_t i = state.processed_candles;
       i < symbol_aggregations.one_minute.size();
       ++i) {
    const Candle& candle = symbol_aggregations.one_minute[i].candle;
    // Skip synthetic candles that do not carry a timestamp.
    if (candle.opened_at().seconds() == 0 && candle.opened_at().nanos() == 0) {
      continue;
    }

    const std::chrono::system_clock::time_point opened_at =
        to_std_chrono(candle.opened_at());
    if (!is_regular_market_hours(opened_at)) continue;

    const std::chrono::local_days trading_day = market_trading_day(opened_at);
    const int minutes_open = minutes_since_market_open(opened_at);

    if (!state.current_session_day.has_value()) {
      _start_new_session(state, trading_day, candle.open());
    } else if (trading_day > *state.current_session_day) {
      if (state.has_opening_window_data) {
        state.prior_opening_volumes.push_back(state.opening_window_volume);
      }
      _start_new_session(state, trading_day, candle.open());
    } else if (trading_day < *state.current_session_day) {
      continue;
    }

    if (minutes_open >= 0 && minutes_open < _options.opening_window_minutes) {
      state.opening_window_volume += static_cast<double>(candle.volume());
      if (!state.has_opening_window_data) {
        state.opening_range_high = candle.high();
        state.opening_range_low = candle.low();
        state.has_opening_window_data = true;
      } else {
        state.opening_range_high =
            std::max(state.opening_range_high, candle.high());
        state.opening_range_low =
            std::min(state.opening_range_low, candle.low());
      }
    }
  }

  state.processed_candles = symbol_aggregations.one_minute.size();
}

decision gap_momentum_analyzer::_evaluate_gap_signal(
    stock::Symbol symbol,
    const trading_state& data,
    const _symbol_session_state& state,
    double gap_percent,
    double relative_volume,
    double latest_close) const {
  // Gap and Go: high-volume gap up with breakout above the opening range high.
  if (gap_percent > _options.momentum_gap_threshold &&
      relative_volume > _options.high_relative_volume) {
    if (latest_close > state.opening_range_high && can_buy(symbol, data)) {
      const double gap_score = gap_percent / _options.momentum_gap_threshold;
      const double volume_score =
          relative_volume / _options.high_relative_volume;
      const double confidence =
          std::clamp(0.25 * (gap_score + volume_score), 0.0, 1.0);
      return {.act = action::BUY, .confidence = confidence};
    }
    return NO_ACTION;
  }

  // Gap Fill: large gap up on weak relative volume indicating mean reversion.
  if (gap_percent > _options.fill_gap_threshold &&
      relative_volume < _options.low_relative_volume) {
    const double confidence =
        std::clamp(gap_percent / _options.fill_gap_threshold * 0.5, 0.0, 1.0);
    return {
        .act = can_sell(symbol, data) ? action::SELL : action::HOLD,
        .confidence = confidence};
  }

  return NO_ACTION;
}

decision gap_momentum_analyzer::analyze(
    stock::Symbol symbol, const trading_state& data) {
  const aggregations* symbol_aggregations = find_aggregations(symbol, data);
  if (!symbol_aggregations || symbol_aggregations->one_minute.empty()) {
    return NO_ACTION;
  }

  auto [itr, inserted] = _sessions.try_emplace(
      symbol, static_cast<uint32_t>(_options.volume_baseline_days));
  _symbol_session_state& state = itr->second;
  _update_state(state, *symbol_aggregations);

  if (!data.market_is_open()) return NO_ACTION;
  if (symbol_aggregations->daily_candles.size() < 2) { return NO_ACTION; }
  const Candle& previous_day = symbol_aggregations->daily_candles(-2);
  const Candle& current_day = symbol_aggregations->daily_candles(-1);
  const std::chrono::local_days current_trading_day =
      market_trading_day(to_std_chrono(current_day.opened_at()));
  if (!state.current_session_day.has_value() ||
      *state.current_session_day != current_trading_day ||
      !state.has_opening_window_data) {
    return NO_ACTION;
  }

  const int minutes_open = minutes_since_market_open(data.time_now.load());
  if (minutes_open < _options.opening_window_minutes ||
      minutes_open > _options.active_window_minutes) {
    return NO_ACTION;
  }

  const double previous_close = previous_day.close();
  if (previous_close <= 0.0) return NO_ACTION;
  if (state.prior_opening_volumes.empty()) return NO_ACTION;

  double volume_sum = 0.0;
  for (const double volume : state.prior_opening_volumes) {
    volume_sum += volume;
  }
  const double average_opening_volume =
      volume_sum / static_cast<double>(state.prior_opening_volumes.size());
  if (average_opening_volume <= 0.0) return NO_ACTION;

  const double gap_percent =
      (state.today_open - previous_close) / previous_close;
  const double relative_volume =
      state.opening_window_volume / average_opening_volume;
  const double latest_close =
      symbol_aggregations->one_minute(-1).candle.close();

  return _evaluate_gap_signal(
      symbol, data, state, gap_percent, relative_volume, latest_close);
}

} // namespace howling
