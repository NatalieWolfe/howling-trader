#include "time/market_time.h"

#include <chrono>
#include <stdexcept>

namespace howling {
namespace {

using duration = std::chrono::system_clock::duration;
using time_point = std::chrono::system_clock::time_point;
using local_time = std::chrono::local_time<duration>;
using zoned_time = std::chrono::zoned_time<duration>;

// Regular US equity market hours offsets from midnight in the market timezone
// (America/New_York).
constexpr std::chrono::minutes MARKET_OPEN_OFFSET =
    std::chrono::hours{9} + std::chrono::minutes{30};
constexpr std::chrono::minutes MARKET_CLOSE_OFFSET = std::chrono::hours{16};

const std::chrono::time_zone* market_time_zone() {
  static const std::chrono::time_zone* const TIME_ZONE = ([]() {
    const std::chrono::time_zone* zone =
        std::chrono::locate_zone(MARKET_TIME_ZONE_NAME);
    if (zone == nullptr) {
      throw std::runtime_error("Failed to locate market time zone.");
    }
    return zone;
  })();
  return TIME_ZONE;
}

local_time to_market_local_time(time_point time) {
  const zoned_time zoned{market_time_zone(), time};
  return zoned.get_local_time();
}

} // namespace

std::chrono::local_days market_trading_day(time_point time) {
  const local_time local = to_market_local_time(time);
  return std::chrono::floor<std::chrono::days>(local);
}

std::chrono::hh_mm_ss<std::chrono::seconds>
market_time_of_day(time_point time) {
  const local_time local = to_market_local_time(time);
  const std::chrono::local_days day =
      std::chrono::floor<std::chrono::days>(local);
  return std::chrono::hh_mm_ss<std::chrono::seconds>{
      std::chrono::floor<std::chrono::seconds>(local - day)};
}

bool is_regular_market_hours(time_point time) {
  const std::chrono::seconds time_of_day =
      market_time_of_day(time).to_duration();
  return (time_of_day >= MARKET_OPEN_OFFSET) &&
      (time_of_day < MARKET_CLOSE_OFFSET);
}

int minutes_since_market_open(time_point time) {
  const std::chrono::seconds time_of_day =
      market_time_of_day(time).to_duration();
  return static_cast<int>(
      std::chrono::floor<std::chrono::minutes>(time_of_day - MARKET_OPEN_OFFSET)
          .count());
}

} // namespace howling
