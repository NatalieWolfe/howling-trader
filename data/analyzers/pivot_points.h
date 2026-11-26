#pragma once

#include <optional>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * @brief Configuration options for pivot_points_analyzer.
 */
struct pivot_points_options {
  // Fractional proximity band around a level (e.g., 0.002 = 0.2%).
  double proximity_threshold = 0.002;
  // Confidence scaling factor when price enters a level band.
  double confidence_scaler = 1.0;
  // Base confidence for major levels (R2 and S2), in (0.0, 1.0].
  double major_confidence = 1.0;
  // Base confidence for minor levels (R1 and S1), in (0.0, 1.0].
  double minor_confidence = 0.7;
};

/**
 * @brief Standard floor-trader pivot point levels derived from a prior session.
 */
struct pivot_levels {
  // Central pivot point (P), calculated as (high + low + close) / 3.
  double pivot;
  // First resistance level (R1), calculated as (2 * pivot) - low.
  double r1;
  // First support level (S1), calculated as (2 * pivot) - high.
  double s1;
  // Second resistance level (R2), calculated as pivot + (high - low).
  double r2;
  // Second support level (S2), calculated as pivot - (high - low).
  double s2;
};

/**
 * @brief Support and resistance analyzer using previous-session pivot points.
 *
 * Computes standard pivot levels (P, R1, S1, R2, S2) from the previous regular
 * trading session's high, low, and close. Emits BUY near support levels when
 * funds are available, SELL near resistance levels when holding a position, and
 * HOLD near resistance levels when flat to act as an entry filter.
 *
 * Thread-safe: immutable after construction.
 */
class pivot_points_analyzer : public analyzer {
public:
  using options = pivot_points_options;

  /**
   * @brief Constructs a pivot points analyzer.
   *
   * @param opts
   *  Configuration options for proximity bands and confidence levels.
   * @throws std::invalid_argument if proximity_threshold is not in (0, 1),
   *  confidence_scaler is not positive and finite, or major_confidence /
   *  minor_confidence is not in (0, 1].
   */
  explicit pivot_points_analyzer(options opts = {});

  /**
   * @brief Computes standard pivot levels from a session's daily candle.
   *
   * @param previous_day Completed regular trading session daily candle.
   * @return Calculated pivot, support, and resistance levels.
   */
  [[nodiscard]] static pivot_levels
  calculate_levels(const Candle& previous_day);

  /**
   * @brief Returns the active pivot levels for a symbol, if available.
   *
   * @param symbol Stock symbol to inspect.
   * @param data Current trading state containing market aggregations.
   * @return Pivot levels if the symbol has a previous session, or std::nullopt.
   */
  [[nodiscard]] std::optional<pivot_levels>
  current_levels(stock::Symbol symbol, const trading_state& data) const;

  /**
   * @brief Evaluates current price against previous-session pivot levels.
   *
   * @param symbol Stock symbol to analyze.
   * @param data Current trading state.
   * @return Trading decision (BUY, SELL, HOLD, or NO_ACTION).
   */
  decision analyze(stock::Symbol symbol, const trading_state& data) override;

private:
  options _options;
};

} // namespace howling
