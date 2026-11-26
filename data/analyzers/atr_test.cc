#include "data/analyzers/atr.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer.pb.h"
#include "data/candle.pb.h"
#include "data/load_analyzer.h"
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

Candle make_candle(double open, double high, double low, double close) {
  Candle candle;
  candle.set_open(open);
  candle.set_high(high);
  candle.set_low(low);
  candle.set_close(close);
  return candle;
}

void push_candle(
    trading_state& state,
    stock::Symbol symbol,
    const Candle& candle,
    int minute_offset) {
  // 14:30 UTC in EST is 09:30 ET.
  state.time_now = make_utc(2026, 11, 20, 14, 30) + minutes{minute_offset};
  add_next_minute(state.market[symbol], candle);
}

TEST(AtrAnalyzerTest, RejectsInvalidOptions) {
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute, atr_analyzer::options{.period = 0}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute, atr_analyzer::options{.period = -3}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute,
          atr_analyzer::options{.baseline_period = 0}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute,
          atr_analyzer::options{.baseline_period = -10}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute,
          atr_analyzer::options{.high_volatility_ratio = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute,
          atr_analyzer::options{.high_volatility_ratio = -1.5}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute,
          atr_analyzer::options{
              .high_volatility_ratio =
                  std::numeric_limits<double>::quiet_NaN()}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(
          &aggregations::one_minute,
          atr_analyzer::options{
              .high_volatility_ratio =
                  std::numeric_limits<double>::infinity()}),
      std::invalid_argument);
  EXPECT_THROW(
      atr_analyzer(nullptr, atr_analyzer::options{}), std::invalid_argument);

  atr_analyzer analyzer;
  EXPECT_THROW(
      (void)analyzer.stop_loss_distance(stock::AAPL, 0.0),
      std::invalid_argument);
  EXPECT_THROW(
      (void)analyzer.stop_loss_distance(stock::AAPL, -2.0),
      std::invalid_argument);
  EXPECT_THROW(
      (void)analyzer.stop_loss_distance(
          stock::AAPL, std::numeric_limits<double>::infinity()),
      std::invalid_argument);
}

TEST(AtrAnalyzerTest, MissingSymbolAndEmptyMarketReturnNoAction) {
  atr_analyzer analyzer;
  trading_state state{};

  const decision missing_result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(missing_result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(missing_result.confidence, 0.0);
  EXPECT_FALSE(analyzer.current_atr(stock::AAPL).has_value());
  EXPECT_FALSE(analyzer.stop_loss_distance(stock::AAPL).has_value());

  // Symbol exists in market map but has no candles yet.
  state.market[stock::AAPL];
  const decision empty_result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(empty_result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(empty_result.confidence, 0.0);
  EXPECT_FALSE(analyzer.current_atr(stock::AAPL).has_value());
}

TEST(AtrAnalyzerTest, WarmupAndHandCalculatedWilderSeriesWithGaps) {
  atr_analyzer analyzer(
      &aggregations::one_minute,
      atr_analyzer::options{
          .period = 3, .baseline_period = 10, .high_volatility_ratio = 1.5});
  trading_state state{};

  // Bar 0: H=102, L=98, C=100 -> TR = 102 - 98 = 4.0 (warmup 1/3).
  push_candle(state, stock::AAPL, make_candle(100.0, 102.0, 98.0, 100.0), 0);
  decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(result.confidence, 0.0);
  EXPECT_FALSE(analyzer.current_atr(stock::AAPL).has_value());
  EXPECT_FALSE(analyzer.stop_loss_distance(stock::AAPL, 2.0).has_value());

  // Bar 1 (gap-up): prev_close=100, H=110, L=105, C=108.
  // TR = max(110-105=5, |110-100|=10, |105-100|=5) = 10.0 (warmup 2/3).
  push_candle(state, stock::AAPL, make_candle(106.0, 110.0, 105.0, 108.0), 1);
  result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(result.confidence, 0.0);
  EXPECT_FALSE(analyzer.current_atr(stock::AAPL).has_value());

  // Bar 2 (gap-down): prev_close=108, H=104, L=98, C=100.
  // TR = max(104-98=6, |104-108|=4, |98-108|=10) = 10.0 (warmup 3/3 -> seed).
  // Seed ATR = (4.0 + 10.0 + 10.0) / 3.0 = 8.0.
  push_candle(state, stock::AAPL, make_candle(102.0, 104.0, 98.0, 100.0), 2);
  result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(result.confidence, 0.0);
  ASSERT_TRUE(analyzer.current_atr(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::AAPL), 8.0);
  ASSERT_TRUE(analyzer.stop_loss_distance(stock::AAPL, 2.5).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.stop_loss_distance(stock::AAPL, 2.5), 20.0);

  // Bar 3: prev_close=100, H=101, L=99, C=100 -> TR = 2.0.
  // Wilder RMA = ((8.0 * 2) + 2.0) / 3.0 = 6.0.
  // Baseline = [8.0, 6.0], avg = 7.0, ratio = 6.0 / 7.0.
  // Confidence = 1.0 - 6.0 / 7.0 = 1.0 / 7.0.
  push_candle(state, stock::AAPL, make_candle(100.0, 101.0, 99.0, 100.0), 3);
  result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_NEAR(result.confidence, 1.0 / 7.0, 1e-12);
  ASSERT_TRUE(analyzer.current_atr(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::AAPL), 6.0);
  EXPECT_DOUBLE_EQ(*analyzer.stop_loss_distance(stock::AAPL), 12.0);
}

TEST(AtrAnalyzerTest, FlatCandlesAvoidDivisionByZeroAndNan) {
  atr_analyzer analyzer(
      &aggregations::one_minute,
      atr_analyzer::options{
          .period = 2, .baseline_period = 5, .high_volatility_ratio = 1.5});
  trading_state state{};

  for (int i = 0; i < 5; ++i) {
    push_candle(state, stock::AAPL, make_candle(100.0, 100.0, 100.0, 100.0), i);
    const decision result = analyzer.analyze(stock::AAPL, state);
    EXPECT_EQ(result.act, action::NO_ACTION);
    EXPECT_FALSE(std::isnan(result.confidence));
    EXPECT_FALSE(std::isinf(result.confidence));
    if (i >= 1) {
      EXPECT_DOUBLE_EQ(result.confidence, 1.0);
      ASSERT_TRUE(analyzer.current_atr(stock::AAPL).has_value());
      EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::AAPL), 0.0);
    }
  }
}

TEST(AtrAnalyzerTest, VolatilitySpikeTriggersHoldAndBaselineEvictsOldValues) {
  atr_analyzer analyzer(
      &aggregations::one_minute,
      atr_analyzer::options{
          .period = 2, .baseline_period = 4, .high_volatility_ratio = 1.5});
  trading_state state{};

  // Establish a steady ATR of 2.0 across 5 bars (baseline full of [2, 2, 2,
  // 2]).
  for (int i = 0; i < 5; ++i) {
    push_candle(state, stock::AAPL, make_candle(100.0, 101.0, 99.0, 100.0), i);
    const decision result = analyzer.analyze(stock::AAPL, state);
    EXPECT_EQ(result.act, action::NO_ACTION);
  }
  ASSERT_TRUE(analyzer.current_atr(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::AAPL), 2.0);

  // Introduce a large volatility spike: H=110, L=90, C=100 -> TR = 20.0.
  // New ATR = ((2.0 * 1) + 20.0) / 2.0 = 11.0.
  // Baseline becomes [2.0, 2.0, 2.0, 11.0], avg = 17.0 / 4.0 = 4.25.
  // Ratio = 11.0 / 4.25 = 44.0 / 17.0 (~2.588 >= 1.5).
  // Hold confidence = clamp((ratio - 1.0) / 1.5, 0.0, 1.0) = 1.0.
  push_candle(state, stock::AAPL, make_candle(100.0, 110.0, 90.0, 100.0), 5);
  const decision spike = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(spike.act, action::HOLD);
  EXPECT_DOUBLE_EQ(spike.confidence, 1.0);
  EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::AAPL), 11.0);
}

TEST(AtrAnalyzerTest, IsolatesStateAcrossMultipleSymbols) {
  atr_analyzer analyzer(
      &aggregations::one_minute,
      atr_analyzer::options{
          .period = 2, .baseline_period = 5, .high_volatility_ratio = 1.5});
  trading_state state{};

  // Interleave low-volatility AAPL (TR = 2.0) and high-volatility NVDA (TR
  // = 10.0).
  push_candle(state, stock::AAPL, make_candle(100.0, 101.0, 99.0, 100.0), 0);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  push_candle(state, stock::NVDA, make_candle(200.0, 205.0, 195.0, 200.0), 0);
  EXPECT_EQ(analyzer.analyze(stock::NVDA, state).act, action::NO_ACTION);

  push_candle(state, stock::AAPL, make_candle(100.0, 101.0, 99.0, 100.0), 1);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  push_candle(state, stock::NVDA, make_candle(200.0, 205.0, 195.0, 200.0), 1);
  EXPECT_EQ(analyzer.analyze(stock::NVDA, state).act, action::NO_ACTION);

  ASSERT_TRUE(analyzer.current_atr(stock::AAPL).has_value());
  ASSERT_TRUE(analyzer.current_atr(stock::NVDA).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::AAPL), 2.0);
  EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::NVDA), 10.0);
}

TEST(AtrAnalyzerTest, MultiMinuteWindowStepsOnlyOnFullWindowBoundaries) {
  atr_analyzer analyzer(
      &aggregations::five_minute,
      atr_analyzer::options{
          .period = 2, .baseline_period = 5, .high_volatility_ratio = 1.5});
  trading_state state{};

  // Minutes 1..4 (09:31..09:34): partial 5m windows, should be ignored.
  for (int i = 1; i <= 4; ++i) {
    push_candle(state, stock::AAPL, make_candle(100.0, 102.0, 98.0, 100.0), i);
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
    EXPECT_FALSE(analyzer.current_atr(stock::AAPL).has_value());
  }

  // Minute 5 (09:35): first full 5m boundary (count == 5), TR = 4.0 (warmup
  // 1/2).
  push_candle(state, stock::AAPL, make_candle(100.0, 102.0, 98.0, 100.0), 5);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_FALSE(analyzer.current_atr(stock::AAPL).has_value());

  // Minutes 6..9 (09:36..09:39): non-boundary minutes, must not advance RMA.
  for (int i = 6; i <= 9; ++i) {
    push_candle(state, stock::AAPL, make_candle(100.0, 103.0, 97.0, 100.0), i);
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
    EXPECT_FALSE(analyzer.current_atr(stock::AAPL).has_value());
  }

  // Minute 10 (09:40): second full 5m boundary, H=103, L=97, prev_close=100 ->
  // TR = 6.0. Seed ATR = (4.0 + 6.0) / 2.0 = 5.0.
  push_candle(state, stock::AAPL, make_candle(100.0, 103.0, 97.0, 100.0), 10);
  const decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  ASSERT_TRUE(analyzer.current_atr(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_atr(stock::AAPL), 5.0);
}

TEST(AtrAnalyzerTest, LoadAnalyzerByNameAndProto) {
  for (const std::string_view name : {"atr", "atr1", "atr5", "atr20"}) {
    std::unique_ptr<analyzer> by_name = load_analyzer(name);
    ASSERT_NE(by_name, nullptr) << name;
    EXPECT_NE(dynamic_cast<atr_analyzer*>(by_name.get()), nullptr) << name;
  }

  Analyzer unspecified_period_proto;
  unspecified_period_proto.mutable_atr();
  EXPECT_THROW(
      (void)load_analyzer(unspecified_period_proto), std::invalid_argument);

  Analyzer default_proto;
  default_proto.mutable_atr()->set_candle_period(Analyzer::ONE);
  std::unique_ptr<analyzer> from_default_proto = load_analyzer(default_proto);
  ASSERT_NE(from_default_proto, nullptr);
  EXPECT_NE(dynamic_cast<atr_analyzer*>(from_default_proto.get()), nullptr);

  Analyzer custom_proto;
  Analyzer::Atr* atr_config = custom_proto.mutable_atr();
  atr_config->set_period(2);
  atr_config->set_baseline_period(4);
  atr_config->set_high_volatility_ratio(1.5);
  atr_config->set_candle_period(Analyzer::ONE);
  std::unique_ptr<analyzer> from_custom_proto = load_analyzer(custom_proto);
  ASSERT_NE(from_custom_proto, nullptr);
  auto* typed = dynamic_cast<atr_analyzer*>(from_custom_proto.get());
  ASSERT_NE(typed, nullptr);

  trading_state state{};
  push_candle(state, stock::AAPL, make_candle(100.0, 102.0, 98.0, 100.0), 0);
  EXPECT_EQ(typed->analyze(stock::AAPL, state).act, action::NO_ACTION);
  push_candle(state, stock::AAPL, make_candle(100.0, 104.0, 96.0, 100.0), 1);
  EXPECT_EQ(typed->analyze(stock::AAPL, state).act, action::NO_ACTION);
  ASSERT_TRUE(typed->current_atr(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*typed->current_atr(stock::AAPL), 6.0);

  Analyzer invalid_proto;
  invalid_proto.mutable_atr()->set_candle_period(Analyzer::ONE);
  invalid_proto.mutable_atr()->set_period(-1);
  EXPECT_THROW((void)load_analyzer(invalid_proto), std::invalid_argument);
}

} // namespace
} // namespace howling
