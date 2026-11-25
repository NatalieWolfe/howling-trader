#include "data/analyzers/pivot_points.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer_utils.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

pivot_points_analyzer::pivot_points_analyzer(options opts) : _options(opts) {
  if (std::isnan(_options.proximity_threshold) ||
      _options.proximity_threshold <= 0.0 ||
      _options.proximity_threshold >= 1.0) {
    throw std::invalid_argument(
        "Pivot points proximity_threshold must be in (0.0, 1.0).");
  }
  if (!std::isfinite(_options.confidence_scaler) ||
      _options.confidence_scaler <= 0.0) {
    throw std::invalid_argument(
        "Pivot points confidence_scaler must be positive and finite.");
  }
  if (std::isnan(_options.major_confidence) ||
      _options.major_confidence <= 0.0 || _options.major_confidence > 1.0) {
    throw std::invalid_argument(
        "Pivot points major_confidence must be in (0.0, 1.0].");
  }
  if (std::isnan(_options.minor_confidence) ||
      _options.minor_confidence <= 0.0 || _options.minor_confidence > 1.0) {
    throw std::invalid_argument(
        "Pivot points minor_confidence must be in (0.0, 1.0].");
  }
}

pivot_levels
pivot_points_analyzer::calculate_levels(const Candle& previous_day) {
  const double pivot =
      (previous_day.high() + previous_day.low() + previous_day.close()) / 3.0;
  const double range = previous_day.high() - previous_day.low();
  return pivot_levels{
      .pivot = pivot,
      .r1 = (2.0 * pivot) - previous_day.low(),
      .s1 = (2.0 * pivot) - previous_day.high(),
      .r2 = pivot + range,
      .s2 = pivot - range,
  };
}

std::optional<pivot_levels> pivot_points_analyzer::current_levels(
    stock::Symbol symbol, const trading_state& data) const {
  const aggregations* symbol_aggregations = find_aggregations(symbol, data);
  if (symbol_aggregations == nullptr ||
      symbol_aggregations->daily_candles.size() < 2) {
    return std::nullopt;
  }
  return calculate_levels(symbol_aggregations->daily_candles(-2));
}

decision pivot_points_analyzer::analyze(
    stock::Symbol symbol, const trading_state& data) {
  const aggregations* symbol_aggregations = find_aggregations(symbol, data);
  if (symbol_aggregations == nullptr ||
      symbol_aggregations->one_minute.empty() ||
      symbol_aggregations->daily_candles.size() < 2) {
    return NO_ACTION;
  }

  const Candle& previous_day = symbol_aggregations->daily_candles(-2);
  if (previous_day.high() <= previous_day.low()) {
    // A flat prior session has zero range, so all pivot levels collapse.
    return NO_ACTION;
  }

  const pivot_levels levels = calculate_levels(previous_day);
  const double price = symbol_aggregations->one_minute(-1).candle.close();
  const double threshold = _options.proximity_threshold;
  const double major_confidence = std::clamp(
      _options.major_confidence * _options.confidence_scaler, 0.0, 1.0);
  const double minor_confidence = std::clamp(
      _options.minor_confidence * _options.confidence_scaler, 0.0, 1.0);

  // Resistance: take profit when holding a position, or HOLD to filter entries.
  if (price >= levels.r2 * (1.0 - threshold)) {
    const action selected_action =
        can_sell(symbol, data) ? action::SELL : action::HOLD;
    return {.act = selected_action, .confidence = major_confidence};
  }
  if (price >= levels.r1 * (1.0 - threshold) &&
      price <= levels.r1 * (1.0 + threshold)) {
    const action selected_action =
        can_sell(symbol, data) ? action::SELL : action::HOLD;
    return {.act = selected_action, .confidence = minor_confidence};
  }

  // Support: emit BUY when funds are available.
  if (price <= levels.s2 * (1.0 + threshold)) {
    if (can_buy(symbol, data)) {
      return {.act = action::BUY, .confidence = major_confidence};
    }
    return NO_ACTION;
  }
  if (price >= levels.s1 * (1.0 - threshold) &&
      price <= levels.s1 * (1.0 + threshold)) {
    if (can_buy(symbol, data)) {
      return {.act = action::BUY, .confidence = minor_confidence};
    }
    return NO_ACTION;
  }

  return NO_ACTION;
}

} // namespace howling
