#include "data/analyzer_utils.h"

#include <chrono>
#include <stdexcept>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.pb.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::day;
using ::std::chrono::hours;
using ::std::chrono::minutes;
using ::std::chrono::month;
using ::std::chrono::sys_days;
using ::std::chrono::system_clock;
using ::std::chrono::year;

system_clock::time_point make_utc(
    int year_value,
    unsigned month_value,
    unsigned day_value,
    int hour_value,
    int minute_value) {
  return sys_days{year{year_value} / month{month_value} / day{day_value}} +
      hours{hour_value} + minutes{minute_value};
}

TEST(AnalyzerUtilsTest, FindAggregationsReturnsNullWhenMissing) {
  trading_state state{};
  EXPECT_EQ(find_aggregations(stock::AAPL, state), nullptr);

  aggregations& inserted = state.market[stock::AAPL];
  EXPECT_EQ(find_aggregations(stock::AAPL, state), &inserted);
  EXPECT_EQ(find_aggregations(stock::NVDA, state), nullptr);
}

TEST(AnalyzerUtilsTest, WindowSizeForHandlesEmptyAndPopulatedSeries) {
  vector<window> empty_series;
  EXPECT_EQ(window_size_for(empty_series), 0);

  aggregations aggr;
  for (int i = 0; i < 7; ++i) {
    Candle candle;
    candle.set_open(100.0 + i);
    candle.set_high(101.0 + i);
    candle.set_low(99.0 + i);
    candle.set_close(100.5 + i);
    add_next_minute(aggr, candle);
  }

  EXPECT_EQ(window_size_for(aggr.one_minute), 1);
  EXPECT_EQ(window_size_for(aggr.five_minute), 5);
  EXPECT_EQ(window_size_for(aggr.twenty_minute), 7);
}

TEST(AnalyzerUtilsTest, ExpectedWindowSizeForMapsKnownPeriodsAndRejectsNull) {
  EXPECT_EQ(expected_window_size_for(&aggregations::one_minute), 1);
  EXPECT_EQ(expected_window_size_for(&aggregations::five_minute), 5);
  EXPECT_EQ(expected_window_size_for(&aggregations::twenty_minute), 20);
  EXPECT_THROW((void)expected_window_size_for(nullptr), std::invalid_argument);
}

TEST(AnalyzerUtilsTest, IsWindowStepBoundaryChecksMinuteAlignment) {
  trading_state state{};
  // 14:33 UTC in EST is 09:33 ET (minute = 33).
  state.time_now = make_utc(2026, 11, 20, 14, 33);
  EXPECT_THROW(
      (void)is_window_step_boundary(state, minutes{-1}), std::invalid_argument);
  EXPECT_THROW(
      (void)is_window_step_boundary(state, minutes{0}), std::invalid_argument);
  EXPECT_THROW(
      (void)is_window_step_boundary(state, minutes{60}), std::invalid_argument);
  EXPECT_THROW(
      (void)is_window_step_boundary(state, minutes{120}),
      std::invalid_argument);
  EXPECT_TRUE(is_window_step_boundary(state, minutes{1}));
  EXPECT_FALSE(is_window_step_boundary(state, minutes{5}));
  EXPECT_FALSE(is_window_step_boundary(state, minutes{20}));

  // 14:35 UTC in EST is 09:35 ET (minute = 35).
  state.time_now = make_utc(2026, 11, 20, 14, 35);
  EXPECT_TRUE(is_window_step_boundary(state, minutes{1}));
  EXPECT_TRUE(is_window_step_boundary(state, minutes{5}));
  EXPECT_FALSE(is_window_step_boundary(state, minutes{20}));

  // 14:40 UTC in EST is 09:40 ET (minute = 40).
  state.time_now = make_utc(2026, 11, 20, 14, 40);
  EXPECT_TRUE(is_window_step_boundary(state, minutes{1}));
  EXPECT_TRUE(is_window_step_boundary(state, minutes{5}));
  EXPECT_TRUE(is_window_step_boundary(state, minutes{20}));
}

TEST(AnalyzerUtilsTest, PeriodMemberMapsValidAndRejectsInvalidPeriods) {
  EXPECT_EQ(period_member(Analyzer::ONE), &aggregations::one_minute);
  EXPECT_EQ(period_member(Analyzer::FIVE), &aggregations::five_minute);
  EXPECT_EQ(period_member(Analyzer::TWENTY), &aggregations::twenty_minute);

  EXPECT_THROW(
      (void)period_member(Analyzer::PERIOD_UNSPECIFIED), std::invalid_argument);
  EXPECT_THROW(
      (void)period_member(static_cast<Analyzer::Period>(999)),
      std::invalid_argument);
}

} // namespace
} // namespace howling
