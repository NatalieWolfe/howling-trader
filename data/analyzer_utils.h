#pragma once

#include <chrono>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.pb.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

/**
 * @brief Finds the market aggregations for the given stock symbol.
 *
 * @param symbol Stock symbol to look up.
 * @param data Current trading state containing market aggregations.
 * @return Pointer to the symbol's aggregations, or nullptr if not present.
 */
[[nodiscard]] const aggregations*
find_aggregations(stock::Symbol symbol, const trading_state& data);

/**
 * @brief Returns the window size of the most recent entry in a period series.
 *
 * @param period Series of aggregated windows.
 * @return The latest window's count, or 0 if the series is empty.
 */
[[nodiscard]] int window_size_for(const vector<window>& period);

/**
 * @brief Returns true if the current market minute is divisible by
 * `window_minutes`.
 *
 * @param data Current trading state supplying the market minute.
 * @param window_minutes Size of the aggregation window in minutes.
 * @return True if the current market minute is divisible by `window_minutes`.
 * @throws std::invalid_argument if `window_minutes` is not in (0, 60).
 */
[[nodiscard]] bool is_window_step_boundary(
    const trading_state& data, std::chrono::minutes window_minutes);

/**
 * @brief Maps a shared Analyzer::Period enum value to the corresponding
 * aggregations member pointer.
 *
 * @param period Configured aggregation period enum value.
 * @return Pointer to the selected window series member of aggregations.
 * @throws std::invalid_argument if period is unspecified or not a recognized
 * enum value.
 */
[[nodiscard]] vector<window> aggregations::*
period_member(Analyzer::Period period);

} // namespace howling
