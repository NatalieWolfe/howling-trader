#include "time/market_time.h"

#include <chrono>

#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::day;
using ::std::chrono::hh_mm_ss;
using ::std::chrono::hours;
using ::std::chrono::local_days;
using ::std::chrono::minutes;
using ::std::chrono::month;
using ::std::chrono::seconds;
using ::std::chrono::sys_days;
using ::std::chrono::system_clock;
using ::std::chrono::year;

system_clock::time_point make_utc(
    int year_value,
    unsigned month_value,
    unsigned day_value,
    int hour_value,
    int minute_value,
    int second_value = 0) {
  return sys_days{year{year_value} / month{month_value} / day{day_value}} +
      hours{hour_value} + minutes{minute_value} + seconds{second_value};
}

TEST(MarketTimeTest, EstMarketOpenAndCloseBounds) {
  // 2026-11-20 is in EST (UTC-5).
  const system_clock::time_point pre_market =
      make_utc(2026, 11, 20, 14, 29, 59);
  const hh_mm_ss<seconds> pre_hms = market_time_of_day(pre_market);
  EXPECT_EQ(pre_hms.hours().count(), 9);
  EXPECT_EQ(pre_hms.minutes().count(), 29);
  EXPECT_EQ(pre_hms.seconds().count(), 59);
  EXPECT_FALSE(is_regular_market_hours(pre_market));
  EXPECT_EQ(minutes_since_market_open(pre_market), -1);

  const system_clock::time_point open_time = make_utc(2026, 11, 20, 14, 30, 0);
  const hh_mm_ss<seconds> open_hms = market_time_of_day(open_time);
  EXPECT_EQ(open_hms.hours().count(), 9);
  EXPECT_EQ(open_hms.minutes().count(), 30);
  EXPECT_EQ(open_hms.seconds().count(), 0);
  EXPECT_TRUE(is_regular_market_hours(open_time));
  EXPECT_EQ(minutes_since_market_open(open_time), 0);

  const system_clock::time_point last_minute =
      make_utc(2026, 11, 20, 20, 59, 59);
  const hh_mm_ss<seconds> last_hms = market_time_of_day(last_minute);
  EXPECT_EQ(last_hms.hours().count(), 15);
  EXPECT_EQ(last_hms.minutes().count(), 59);
  EXPECT_EQ(last_hms.seconds().count(), 59);
  EXPECT_TRUE(is_regular_market_hours(last_minute));
  EXPECT_EQ(minutes_since_market_open(last_minute), 389);

  const system_clock::time_point close_time = make_utc(2026, 11, 20, 21, 0, 0);
  const hh_mm_ss<seconds> close_hms = market_time_of_day(close_time);
  EXPECT_EQ(close_hms.hours().count(), 16);
  EXPECT_EQ(close_hms.minutes().count(), 0);
  EXPECT_EQ(close_hms.seconds().count(), 0);
  EXPECT_FALSE(is_regular_market_hours(close_time));
  EXPECT_EQ(minutes_since_market_open(close_time), 390);
}

TEST(MarketTimeTest, EdtMarketOpenAndCloseBounds) {
  // 2026-07-15 is in EDT (UTC-4).
  const system_clock::time_point pre_market = make_utc(2026, 7, 15, 13, 29, 0);
  EXPECT_FALSE(is_regular_market_hours(pre_market));
  EXPECT_EQ(minutes_since_market_open(pre_market), -1);

  const system_clock::time_point open_time = make_utc(2026, 7, 15, 13, 30, 0);
  const hh_mm_ss<seconds> open_hms = market_time_of_day(open_time);
  EXPECT_EQ(open_hms.hours().count(), 9);
  EXPECT_EQ(open_hms.minutes().count(), 30);
  EXPECT_TRUE(is_regular_market_hours(open_time));
  EXPECT_EQ(minutes_since_market_open(open_time), 0);

  const system_clock::time_point last_minute = make_utc(2026, 7, 15, 19, 59, 0);
  EXPECT_TRUE(is_regular_market_hours(last_minute));
  EXPECT_EQ(minutes_since_market_open(last_minute), 389);

  const system_clock::time_point close_time = make_utc(2026, 7, 15, 20, 0, 0);
  EXPECT_FALSE(is_regular_market_hours(close_time));
  EXPECT_EQ(minutes_since_market_open(close_time), 390);
}

TEST(MarketTimeTest, DstSpringForwardAndFallBackTransitions) {
  // 2026-03-08 is US DST spring forward (EST -> EDT at 02:00 local).
  // 2026-03-06 (Friday, EST): 14:30 UTC is 09:30 ET.
  EXPECT_TRUE(is_regular_market_hours(make_utc(2026, 3, 6, 14, 30, 0)));
  EXPECT_FALSE(is_regular_market_hours(make_utc(2026, 3, 6, 13, 30, 0)));

  // 2026-03-09 (Monday, EDT): 13:30 UTC is 09:30 ET.
  EXPECT_TRUE(is_regular_market_hours(make_utc(2026, 3, 9, 13, 30, 0)));
  EXPECT_EQ(minutes_since_market_open(make_utc(2026, 3, 9, 13, 30, 0)), 0);

  // 2026-11-01 is US DST fall back (EDT -> EST at 02:00 local).
  // 2026-10-30 (Friday, EDT): 13:30 UTC is 09:30 ET.
  EXPECT_TRUE(is_regular_market_hours(make_utc(2026, 10, 30, 13, 30, 0)));

  // 2026-11-02 (Monday, EST): 13:30 UTC is 08:30 ET, 14:30 UTC is 09:30 ET.
  EXPECT_FALSE(is_regular_market_hours(make_utc(2026, 11, 2, 13, 30, 0)));
  EXPECT_EQ(minutes_since_market_open(make_utc(2026, 11, 2, 13, 30, 0)), -60);
  EXPECT_TRUE(is_regular_market_hours(make_utc(2026, 11, 2, 14, 30, 0)));
  EXPECT_EQ(minutes_since_market_open(make_utc(2026, 11, 2, 14, 30, 0)), 0);
}

TEST(MarketTimeTest, TradingDayRolloverUsesMarketMidnight) {
  // In EST (UTC-5), 04:59:59 UTC on Nov 21 is 23:59:59 ET on Nov 20.
  const system_clock::time_point before_midnight =
      make_utc(2026, 11, 21, 4, 59, 59);
  const local_days nov_20{year{2026} / month{11} / day{20}};
  const local_days nov_21{year{2026} / month{11} / day{21}};
  EXPECT_EQ(market_trading_day(before_midnight), nov_20);

  // 05:00:00 UTC on Nov 21 is 00:00:00 ET on Nov 21.
  const system_clock::time_point at_midnight = make_utc(2026, 11, 21, 5, 0, 0);
  EXPECT_EQ(market_trading_day(at_midnight), nov_21);
  EXPECT_EQ(minutes_since_market_open(at_midnight), -570);

  // In EDT (UTC-4), 03:59:59 UTC on Jul 16 is 23:59:59 ET on Jul 15.
  const local_days jul_15{year{2026} / month{7} / day{15}};
  const local_days jul_16{year{2026} / month{7} / day{16}};
  EXPECT_EQ(market_trading_day(make_utc(2026, 7, 16, 3, 59, 59)), jul_15);
  EXPECT_EQ(market_trading_day(make_utc(2026, 7, 16, 4, 0, 0)), jul_16);
}

} // namespace
} // namespace howling
