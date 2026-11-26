#include "trading/trading_state.h"

#include <ranges>

#include "time/market_time.h"

namespace howling {

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
  return static_cast<int>(market_time_of_day(time_now.load()).hours().count());
}

int trading_state::market_minute() const {
  return static_cast<int>(
      market_time_of_day(time_now.load()).minutes().count());
}

int trading_state::market_second() const {
  return static_cast<int>(
      market_time_of_day(time_now.load()).seconds().count());
}

bool trading_state::market_is_open() const {
  return is_regular_market_hours(time_now.load());
}

} // namespace howling
