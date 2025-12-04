#include "trading/trading_state.h"

#include <chrono>
#include <ranges>
#include <string_view>

namespace howling {
namespace {

using std::chrono::days;
using std::chrono::hh_mm_ss;
using std::chrono::locate_zone;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::chrono::zoned_time;

constexpr std::string_view MARKET_TIME_ZONE_NAME = "America/New_York";

auto to_market_hms(system_clock::time_point time) {
  static const std::chrono::time_zone* const TIME_ZONE =
      locate_zone(MARKET_TIME_ZONE_NAME);
  zoned_time zoned{TIME_ZONE, time};
  return hh_mm_ss{std::chrono::floor<seconds>(
      zoned.get_local_time() -
      std::chrono::floor<days>(zoned.get_local_time()))};
}

} // namespace

double trading_state::total_positions_cost() const {
  double total = 0;
  for (const position p : positions | std::views::values | std::views::join) {
    total += p.cost();
  }
  return total;
}

double trading_state::total_positions_value() const {
  double total = 0;
  for (const position p : positions | std::views::values | std::views::join) {
    total += p.quantity * market.at(p.symbol).one_minute(-1).candle.close();
  }
  return total;
}

int trading_state::market_hour() const {
  return to_market_hms(time_now).hours().count();
}

int trading_state::market_minute() const {
  return to_market_hms(time_now).minutes().count();
}

int trading_state::market_second() const {
  return to_market_hms(time_now).seconds().count();
}

bool trading_state::market_is_open() const {
  const hh_mm_ss hms = to_market_hms(time_now);
  if (hms.hours().count() < 6 || hms.hours().count() >= 16) return false;
  if (hms.hours().count() == 6 && hms.minutes().count() < 30) return false;
  return true;
}

} // namespace howling
