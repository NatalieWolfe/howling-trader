#pragma once

#include <chrono>
#include <string_view>

namespace howling {

// IANA timezone identifier for US equity market hours.
inline constexpr std::string_view MARKET_TIME_ZONE_NAME = "America/New_York";

/**
 * @brief Returns the local calendar trading day in the market timezone.
 *
 * @param time UTC timestamp to convert.
 * @return Local calendar date in the America/New_York timezone.
 */
[[nodiscard]] std::chrono::local_days
market_trading_day(std::chrono::system_clock::time_point time);

/**
 * @brief Returns the local time of day in the market timezone, truncated to
 * seconds.
 *
 * @param time UTC timestamp to convert.
 * @return Hours, minutes, and seconds since midnight in America/New_York.
 */
[[nodiscard]] std::chrono::hh_mm_ss<std::chrono::seconds>
market_time_of_day(std::chrono::system_clock::time_point time);

/**
 * @brief Returns true if the timestamp falls within regular US equity market
 * hours, [09:30, 16:00) Eastern Time.
 *
 * @param time UTC timestamp to check.
 * @return True if during regular market hours, false otherwise.
 */
[[nodiscard]] bool
is_regular_market_hours(std::chrono::system_clock::time_point time);

/**
 * @brief Returns the signed number of whole minutes elapsed since 09:30 ET on
 * the same market trading day.
 *
 * Pre-market timestamps return negative values (e.g., 09:29:59 ET returns -1).
 *
 * @param time UTC timestamp to evaluate.
 * @return Signed whole minutes relative to 09:30 ET.
 */
[[nodiscard]] int
minutes_since_market_open(std::chrono::system_clock::time_point time);

} // namespace howling
