#include "data/aggregate.h"

#include <chrono>
#include <cstdint>

#include "data/candle.pb.h"
#include "time/conversion.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::day;
using ::std::chrono::hours;
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

Candle make_candle(
    double open,
    double high,
    double low,
    double close,
    int64_t volume,
    system_clock::time_point opened_at,
    system_clock::duration duration = minutes{1}) {
  Candle candle;
  candle.set_open(open);
  candle.set_high(high);
  candle.set_low(low);
  candle.set_close(close);
  candle.set_volume(volume);
  *candle.mutable_opened_at() = to_proto(opened_at);
  *candle.mutable_duration() = to_proto(duration);
  return candle;
}

TEST(AggregateTest, AccumulatesSingleSessionDailyCandle) {
  aggregations aggr;
  // 2026-11-20 is in EST (UTC-5): 14:30 UTC is 09:30 ET.
  const system_clock::time_point first_open = make_utc(2026, 11, 20, 14, 30);
  add_next_minute(
      aggr, make_candle(100.0, 102.0, 99.5, 101.0, 500, first_open));
  add_next_minute(
      aggr,
      make_candle(
          101.0, 105.0, 100.5, 104.0, 800, make_utc(2026, 11, 20, 14, 31)));
  add_next_minute(
      aggr,
      make_candle(
          104.0, 104.5, 98.0, 102.5, 700, make_utc(2026, 11, 20, 15, 0)));

  ASSERT_EQ(aggr.daily_candles.size(), 1u);
  const Candle& daily = aggr.daily_candles(-1);
  EXPECT_EQ(to_std_chrono(daily.opened_at()), first_open);
  EXPECT_EQ(to_std_chrono(daily.duration()), minutes{31});
  EXPECT_DOUBLE_EQ(daily.open(), 100.0);
  EXPECT_DOUBLE_EQ(daily.high(), 105.0);
  EXPECT_DOUBLE_EQ(daily.low(), 98.0);
  EXPECT_DOUBLE_EQ(daily.close(), 102.5);
  EXPECT_EQ(daily.volume(), 2000);
}

TEST(AggregateTest, ExcludesExtendedHoursCandlesFromDailyCandles) {
  aggregations aggr;
  // Pre-market: 09:29 ET (14:29 UTC in EST).
  add_next_minute(
      aggr,
      make_candle(
          50.0, 200.0, 10.0, 55.0, 9000, make_utc(2026, 11, 20, 14, 29)));
  EXPECT_TRUE(aggr.daily_candles.empty());

  // Regular session: 09:30 ET and 15:59 ET.
  const system_clock::time_point regular_open = make_utc(2026, 11, 20, 14, 30);
  add_next_minute(
      aggr, make_candle(100.0, 103.0, 99.0, 102.0, 300, regular_open));
  add_next_minute(
      aggr,
      make_candle(
          102.0, 104.0, 101.0, 103.5, 400, make_utc(2026, 11, 20, 20, 59)));

  // After-hours: 16:00 ET (21:00 UTC in EST).
  add_next_minute(
      aggr,
      make_candle(
          103.5, 300.0, 5.0, 150.0, 9999, make_utc(2026, 11, 20, 21, 0)));

  ASSERT_EQ(aggr.daily_candles.size(), 1u);
  const Candle& daily = aggr.daily_candles(-1);
  EXPECT_EQ(to_std_chrono(daily.opened_at()), regular_open);
  EXPECT_EQ(to_std_chrono(daily.duration()), hours{6} + minutes{30});
  EXPECT_DOUBLE_EQ(daily.open(), 100.0);
  EXPECT_DOUBLE_EQ(daily.high(), 104.0);
  EXPECT_DOUBLE_EQ(daily.low(), 99.0);
  EXPECT_DOUBLE_EQ(daily.close(), 103.5);
  EXPECT_EQ(daily.volume(), 700);
}

TEST(AggregateTest, RollsOverDailyCandlesOnNewTradingDay) {
  aggregations aggr;
  // Day 1: 2026-11-19 (EST).
  const system_clock::time_point day_one_open = make_utc(2026, 11, 19, 14, 30);
  add_next_minute(
      aggr, make_candle(100.0, 106.0, 98.0, 104.0, 1000, day_one_open));
  add_next_minute(
      aggr,
      make_candle(
          104.0, 108.0, 103.0, 107.0, 500, make_utc(2026, 11, 19, 20, 59)));

  // Day 2: 2026-11-20 (EST).
  const system_clock::time_point day_two_open = make_utc(2026, 11, 20, 14, 30);
  add_next_minute(
      aggr, make_candle(110.0, 112.0, 109.0, 111.0, 250, day_two_open));
  add_next_minute(
      aggr,
      make_candle(
          111.0, 115.0, 110.5, 114.0, 350, make_utc(2026, 11, 20, 14, 35)));

  // Late candle from an earlier trading day should be ignored.
  add_next_minute(
      aggr,
      make_candle(
          50.0, 200.0, 10.0, 75.0, 9999, make_utc(2026, 11, 19, 16, 0)));

  ASSERT_EQ(aggr.daily_candles.size(), 2u);

  const Candle& previous_day = aggr.daily_candles(-2);
  EXPECT_EQ(to_std_chrono(previous_day.opened_at()), day_one_open);
  EXPECT_EQ(to_std_chrono(previous_day.duration()), hours{6} + minutes{30});
  EXPECT_DOUBLE_EQ(previous_day.open(), 100.0);
  EXPECT_DOUBLE_EQ(previous_day.high(), 108.0);
  EXPECT_DOUBLE_EQ(previous_day.low(), 98.0);
  EXPECT_DOUBLE_EQ(previous_day.close(), 107.0);
  EXPECT_EQ(previous_day.volume(), 1500);

  const Candle& current_day = aggr.daily_candles(-1);
  EXPECT_EQ(to_std_chrono(current_day.opened_at()), day_two_open);
  EXPECT_EQ(to_std_chrono(current_day.duration()), minutes{6});
  EXPECT_DOUBLE_EQ(current_day.open(), 110.0);
  EXPECT_DOUBLE_EQ(current_day.high(), 115.0);
  EXPECT_DOUBLE_EQ(current_day.low(), 109.0);
  EXPECT_DOUBLE_EQ(current_day.close(), 114.0);
  EXPECT_EQ(current_day.volume(), 600);
}

TEST(AggregateTest, SkipsDailyCandleUpdateWhenTimestampUnset) {
  aggregations aggr;
  Candle synthetic_candle;
  synthetic_candle.set_open(100.0);
  synthetic_candle.set_high(105.0);
  synthetic_candle.set_low(95.0);
  synthetic_candle.set_close(102.0);
  synthetic_candle.set_volume(100);

  add_next_minute(aggr, synthetic_candle);

  EXPECT_EQ(aggr.one_minute.size(), 1u);
  EXPECT_TRUE(aggr.daily_candles.empty());
}

TEST(AggregateTest, HandlesOutOfOrderAndMixedDurationCandlesWithinTradingDay) {
  aggregations aggr;
  // Arrive out of order with mixed durations on 2026-11-20 (EST):
  // 1. Middle 5-minute candle: 09:35 ET - 09:40 ET.
  add_next_minute(
      aggr,
      make_candle(
          105.0,
          108.0,
          104.0,
          107.0,
          400,
          make_utc(2026, 11, 20, 14, 35),
          minutes{5}));
  // 2. Earliest 1-minute candle: 09:30 ET - 09:31 ET.
  const system_clock::time_point earliest_open = make_utc(2026, 11, 20, 14, 30);
  add_next_minute(
      aggr,
      make_candle(100.0, 102.0, 99.0, 101.0, 300, earliest_open, minutes{1}));
  // 3. Latest 15-minute candle: 09:45 ET - 10:00 ET.
  add_next_minute(
      aggr,
      make_candle(
          107.0,
          112.0,
          106.0,
          111.5,
          900,
          make_utc(2026, 11, 20, 14, 45),
          minutes{15}));
  // 4. Interior 2-minute candle: 09:40 ET - 09:42 ET (new low, should not
  // overwrite open or close).
  add_next_minute(
      aggr,
      make_candle(
          103.0,
          104.0,
          97.5,
          103.5,
          200,
          make_utc(2026, 11, 20, 14, 40),
          minutes{2}));

  ASSERT_EQ(aggr.daily_candles.size(), 1u);
  const Candle& daily = aggr.daily_candles(-1);
  EXPECT_EQ(to_std_chrono(daily.opened_at()), earliest_open);
  EXPECT_EQ(to_std_chrono(daily.duration()), minutes{30});
  EXPECT_DOUBLE_EQ(daily.open(), 100.0);
  EXPECT_DOUBLE_EQ(daily.high(), 112.0);
  EXPECT_DOUBLE_EQ(daily.low(), 97.5);
  EXPECT_DOUBLE_EQ(daily.close(), 111.5);
  EXPECT_EQ(daily.volume(), 1800);
}

} // namespace
} // namespace howling
