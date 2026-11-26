#include "time/market_time.h"

#include <chrono>

namespace howling {
namespace {

constexpr std::chrono::minutes MARKET_OPEN_OFFSET =
    std::chrono::hours{9} + std::chrono::minutes{30};
constexpr std::chrono::minutes MARKET_CLOSE_OFFSET = std::chrono::hours{16};

const std::chrono::time_zone* market_time_zone() {
  static const std::chrono::time_zone* const TIME_ZONE =
      std::chrono::locate_zone(MARKET_TIME_ZONE_NAME);
  return TIME_ZONE;
}

std::chrono::local_time<std::chrono::system_clock::duration>
to_market_local_time(std::chrono::system_clock::time_point time) {
  const std::chrono::zoned_time<std::chrono::system_clock::duration> zoned{
      market_time_zone(), time};
  return zoned.get_local_time();
}

} // namespace

std::chrono::local_days
market_trading_day(std::chrono::system_clock::time_point time) {
  const std::chrono::local_time<std::chrono::system_clock::duration> local =
      to_market_local_time(time);
  return std::chrono::floor<std::chrono::days>(local);
}

std::chrono::hh_mm_ss<std::chrono::seconds>
market_time_of_day(std::chrono::system_clock::time_point time) {
  const std::chrono::local_time<std::chrono::system_clock::duration> local =
      to_market_local_time(time);
  const std::chrono::local_days day =
      std::chrono::floor<std::chrono::days>(local);
  return std::chrono::hh_mm_ss<std::chrono::seconds>{
      std::chrono::floor<std::chrono::seconds>(local - day)};
}

bool is_regular_market_hours(std::chrono::system_clock::time_point time) {
  const std::chrono::seconds time_of_day =
      market_time_of_day(time).to_duration();
  return (time_of_day >= MARKET_OPEN_OFFSET) &&
      (time_of_day < MARKET_CLOSE_OFFSET);
}

int minutes_since_market_open(std::chrono::system_clock::time_point time) {
  const std::chrono::seconds time_of_day =
      market_time_of_day(time).to_duration();
  return static_cast<int>(
      std::chrono::floor<std::chrono::minutes>(time_of_day - MARKET_OPEN_OFFSET)
          .count());
}

} // namespace howling
