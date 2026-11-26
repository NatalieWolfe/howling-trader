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

Candle make_candle(
    double open,
    double high,
    double low,
    double close,
    int64_t volume,
    system_clock::time_point opened_at) {
  Candle candle;
  candle.set_open(open);
  candle.set_high(high);
  candle.set_low(low);
  candle.set_close(close);
  candle.set_volume(volume);
  *candle.mutable_opened_at() = to_proto(opened_at);
  return candle;
}

TEST(AggregateTest, AccumulatesSingleSessionOhlc) {
  aggregations aggr;
  // 2026-11-20 is in EST (UTC-5): 14:30 UTC is 09:30 ET.
  add_next_minute(
      aggr,
      make_candle(
          100.0, 102.0, 99.5, 101.0, 500, make_utc(2026, 11, 20, 14, 30)));
  add_next_minute(
      aggr,
      make_candle(
          101.0, 105.0, 100.5, 104.0, 800, make_utc(2026, 11, 20, 14, 31)));
  add_next_minute(
      aggr,
      make_candle(
          104.0, 104.5, 98.0, 102.5, 700, make_utc(2026, 11, 20, 15, 0)));

  ASSERT_TRUE(aggr.current_session.has_value());
  EXPECT_FALSE(aggr.previous_session.has_value());

  const local_days expected_day{year{2026} / month{11} / day{20}};
  EXPECT_EQ(aggr.current_session->trading_day, expected_day);
  EXPECT_DOUBLE_EQ(aggr.current_session->open, 100.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->high, 105.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->low, 98.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->close, 102.5);
  EXPECT_EQ(aggr.current_session->volume, 2000);
}

TEST(AggregateTest, ExcludesExtendedHoursCandlesFromSessionOhlc) {
  aggregations aggr;
  // Pre-market: 09:29 ET (14:29 UTC in EST).
  add_next_minute(
      aggr,
      make_candle(
          50.0, 200.0, 10.0, 55.0, 9000, make_utc(2026, 11, 20, 14, 29)));
  EXPECT_FALSE(aggr.current_session.has_value());

  // Regular session: 09:30 ET and 15:59 ET.
  add_next_minute(
      aggr,
      make_candle(
          100.0, 103.0, 99.0, 102.0, 300, make_utc(2026, 11, 20, 14, 30)));
  add_next_minute(
      aggr,
      make_candle(
          102.0, 104.0, 101.0, 103.5, 400, make_utc(2026, 11, 20, 20, 59)));

  // After-hours: 16:00 ET (21:00 UTC in EST).
  add_next_minute(
      aggr,
      make_candle(
          103.5, 300.0, 5.0, 150.0, 9999, make_utc(2026, 11, 20, 21, 0)));

  ASSERT_TRUE(aggr.current_session.has_value());
  EXPECT_DOUBLE_EQ(aggr.current_session->open, 100.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->high, 104.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->low, 99.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->close, 103.5);
  EXPECT_EQ(aggr.current_session->volume, 700);
}

TEST(AggregateTest, RotatesSessionOnNewTradingDay) {
  aggregations aggr;
  // Day 1: 2026-11-19 (EST).
  add_next_minute(
      aggr,
      make_candle(
          100.0, 106.0, 98.0, 104.0, 1000, make_utc(2026, 11, 19, 14, 30)));
  add_next_minute(
      aggr,
      make_candle(
          104.0, 108.0, 103.0, 107.0, 500, make_utc(2026, 11, 19, 20, 59)));

  // Day 2: 2026-11-20 (EST).
  add_next_minute(
      aggr,
      make_candle(
          110.0, 112.0, 109.0, 111.0, 250, make_utc(2026, 11, 20, 14, 30)));
  add_next_minute(
      aggr,
      make_candle(
          111.0, 115.0, 110.5, 114.0, 350, make_utc(2026, 11, 20, 14, 35)));

  ASSERT_TRUE(aggr.previous_session.has_value());
  ASSERT_TRUE(aggr.current_session.has_value());

  const local_days day_one{year{2026} / month{11} / day{19}};
  const local_days day_two{year{2026} / month{11} / day{20}};

  EXPECT_EQ(aggr.previous_session->trading_day, day_one);
  EXPECT_DOUBLE_EQ(aggr.previous_session->open, 100.0);
  EXPECT_DOUBLE_EQ(aggr.previous_session->high, 108.0);
  EXPECT_DOUBLE_EQ(aggr.previous_session->low, 98.0);
  EXPECT_DOUBLE_EQ(aggr.previous_session->close, 107.0);
  EXPECT_EQ(aggr.previous_session->volume, 1500);

  EXPECT_EQ(aggr.current_session->trading_day, day_two);
  EXPECT_DOUBLE_EQ(aggr.current_session->open, 110.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->high, 115.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->low, 109.0);
  EXPECT_DOUBLE_EQ(aggr.current_session->close, 114.0);
  EXPECT_EQ(aggr.current_session->volume, 600);
}

TEST(AggregateTest, SkipsSessionUpdateWhenTimestampUnset) {
  aggregations aggr;
  Candle synthetic_candle;
  synthetic_candle.set_open(100.0);
  synthetic_candle.set_high(105.0);
  synthetic_candle.set_low(95.0);
  synthetic_candle.set_close(102.0);
  synthetic_candle.set_volume(100);

  add_next_minute(aggr, synthetic_candle);

  EXPECT_EQ(aggr.one_minute.size(), 1u);
  EXPECT_FALSE(aggr.current_session.has_value());
  EXPECT_FALSE(aggr.previous_session.has_value());
}

} // namespace
} // namespace howling
