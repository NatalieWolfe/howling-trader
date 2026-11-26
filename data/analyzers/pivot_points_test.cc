#include "data/analyzers/pivot_points.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer.pb.h"
#include "data/candle.pb.h"
#include "data/load_analyzer.h"
#include "data/stock.pb.h"
#include "time/conversion.h"
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

// Creates a trading_state with a prior session of H=112, L=100, C=103:
// P = 105, R1 = 110, S1 = 98, R2 = 117, S2 = 93.
trading_state make_state_with_price(
    double current_price, double available_funds, bool holding_position) {
  trading_state state{};
  state.available_funds = available_funds;

  aggregations& symbol_aggregations = state.market[stock::AAPL];
  symbol_aggregations.daily_candles.push_back(make_candle(
      104.0, 112.0, 100.0, 103.0, 10000, make_utc(2026, 11, 19, 14, 30)));

  add_next_minute(
      symbol_aggregations,
      make_candle(
          current_price,
          current_price + 0.5,
          current_price - 0.5,
          current_price,
          500,
          make_utc(2026, 11, 20, 15, 0)));

  if (holding_position) {
    state.positions[stock::AAPL].push_back(
        {.symbol = stock::AAPL, .price = 100.0, .quantity = 10});
  }
  return state;
}

TEST(PivotPointsAnalyzerTest, CalculatesExactLevelsFromPreviousSession) {
  const Candle previous = make_candle(
      104.0, 112.0, 100.0, 103.0, 5000, make_utc(2026, 11, 19, 14, 30));

  const pivot_levels levels = pivot_points_analyzer::calculate_levels(previous);
  EXPECT_DOUBLE_EQ(levels.pivot, 105.0);
  EXPECT_DOUBLE_EQ(levels.r1, 110.0);
  EXPECT_DOUBLE_EQ(levels.s1, 98.0);
  EXPECT_DOUBLE_EQ(levels.r2, 117.0);
  EXPECT_DOUBLE_EQ(levels.s2, 93.0);
}

TEST(PivotPointsAnalyzerTest, ReturnsNoActionWhenDataMissingOrDayOne) {
  pivot_points_analyzer analyzer;

  // Missing symbol in market map.
  trading_state empty_state{};
  empty_state.available_funds = 10000.0;
  EXPECT_FALSE(analyzer.current_levels(stock::AAPL, empty_state).has_value());
  decision missing_decision = analyzer.analyze(stock::AAPL, empty_state);
  EXPECT_EQ(missing_decision.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(missing_decision.confidence, 0.0);

  // Symbol exists with previous and current daily candles, but one_minute is
  // empty.
  trading_state no_candles_state{};
  no_candles_state.available_funds = 10000.0;
  no_candles_state.market[stock::AAPL].daily_candles.push_back(make_candle(
      104.0, 112.0, 100.0, 103.0, 5000, make_utc(2026, 11, 19, 14, 30)));
  no_candles_state.market[stock::AAPL].daily_candles.push_back(make_candle(
      103.0, 106.0, 101.0, 104.0, 1000, make_utc(2026, 11, 20, 14, 30)));
  EXPECT_TRUE(
      analyzer.current_levels(stock::AAPL, no_candles_state).has_value());
  decision no_candles_decision =
      analyzer.analyze(stock::AAPL, no_candles_state);
  EXPECT_EQ(no_candles_decision.act, action::NO_ACTION);

  // Day 1: only one daily candle populated, no previous session yet.
  trading_state day_one_state{};
  day_one_state.available_funds = 10000.0;
  add_next_minute(
      day_one_state.market[stock::AAPL],
      make_candle(
          100.0, 105.0, 99.0, 102.0, 500, make_utc(2026, 11, 20, 14, 30)));
  EXPECT_FALSE(analyzer.current_levels(stock::AAPL, day_one_state).has_value());
  decision day_one_decision = analyzer.analyze(stock::AAPL, day_one_state);
  EXPECT_EQ(day_one_decision.act, action::NO_ACTION);
}

TEST(PivotPointsAnalyzerTest, SessionRolloverIgnoresExtendedHoursCandles) {
  trading_state state{};
  state.available_funds = 10000.0;
  aggregations& symbol_aggregations = state.market[stock::AAPL];

  // Day 1 pre-market (08:00 ET = 13:00 UTC in EST) should be ignored.
  add_next_minute(
      symbol_aggregations,
      make_candle(
          50.0, 200.0, 10.0, 60.0, 9000, make_utc(2026, 11, 19, 13, 0)));
  // Day 1 regular hours (09:30 ET and 15:59 ET): H=112, L=100, C=103.
  add_next_minute(
      symbol_aggregations,
      make_candle(
          104.0, 112.0, 100.0, 108.0, 1000, make_utc(2026, 11, 19, 14, 30)));
  add_next_minute(
      symbol_aggregations,
      make_candle(
          108.0, 109.0, 102.0, 103.0, 1000, make_utc(2026, 11, 19, 20, 59)));
  // Day 1 after-hours (17:00 ET = 22:00 UTC in EST) should be ignored.
  add_next_minute(
      symbol_aggregations,
      make_candle(
          103.0, 250.0, 5.0, 180.0, 9000, make_utc(2026, 11, 19, 22, 0)));

  // Day 2 regular hours (09:30 ET) triggers session rollover; price near S1=98.
  add_next_minute(
      symbol_aggregations,
      make_candle(98.5, 99.0, 97.5, 98.0, 500, make_utc(2026, 11, 20, 14, 30)));

  pivot_points_analyzer analyzer;
  std::optional<pivot_levels> levels =
      analyzer.current_levels(stock::AAPL, state);
  ASSERT_TRUE(levels.has_value());
  EXPECT_DOUBLE_EQ(levels->pivot, 105.0);
  EXPECT_DOUBLE_EQ(levels->r1, 110.0);
  EXPECT_DOUBLE_EQ(levels->s1, 98.0);
  EXPECT_DOUBLE_EQ(levels->r2, 117.0);
  EXPECT_DOUBLE_EQ(levels->s2, 93.0);

  decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::BUY);
  EXPECT_DOUBLE_EQ(result.confidence, 0.7);
}

TEST(PivotPointsAnalyzerTest, ResistanceReturnsHoldWhenFlatAndSellWhenHolding) {
  pivot_points_analyzer analyzer;

  // Near R1 (110.0) without position -> HOLD (entry filter).
  decision r1_flat = analyzer.analyze(
      stock::AAPL, make_state_with_price(110.0, 10000.0, false));
  EXPECT_EQ(r1_flat.act, action::HOLD);
  EXPECT_DOUBLE_EQ(r1_flat.confidence, 0.7);

  // Near R1 (110.0) while holding position -> SELL (take profit).
  decision r1_holding = analyzer.analyze(
      stock::AAPL, make_state_with_price(110.0, 10000.0, true));
  EXPECT_EQ(r1_holding.act, action::SELL);
  EXPECT_DOUBLE_EQ(r1_holding.confidence, 0.7);

  // Near R2 (117.0) without position -> HOLD with higher confidence than R1.
  decision r2_flat = analyzer.analyze(
      stock::AAPL, make_state_with_price(116.9, 10000.0, false));
  EXPECT_EQ(r2_flat.act, action::HOLD);
  EXPECT_DOUBLE_EQ(r2_flat.confidence, 1.0);
  EXPECT_GT(r2_flat.confidence, r1_flat.confidence);

  // Above R2 (120.0) while holding position -> SELL with higher confidence.
  decision r2_holding = analyzer.analyze(
      stock::AAPL, make_state_with_price(120.0, 10000.0, true));
  EXPECT_EQ(r2_holding.act, action::SELL);
  EXPECT_DOUBLE_EQ(r2_holding.confidence, 1.0);
  EXPECT_GT(r2_holding.confidence, r1_holding.confidence);
}

TEST(PivotPointsAnalyzerTest, SupportReturnsBuyWhenFundsAvailable) {
  pivot_points_analyzer analyzer;

  // Near S1 (98.0) with funds -> BUY.
  decision s1_buy = analyzer.analyze(
      stock::AAPL, make_state_with_price(98.0, 10000.0, false));
  EXPECT_EQ(s1_buy.act, action::BUY);
  EXPECT_DOUBLE_EQ(s1_buy.confidence, 0.7);

  // Near S1 (98.0) with insufficient funds -> NO_ACTION.
  decision s1_no_funds =
      analyzer.analyze(stock::AAPL, make_state_with_price(98.0, 10.0, false));
  EXPECT_EQ(s1_no_funds.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(s1_no_funds.confidence, 0.0);

  // Near S2 (93.0) with funds -> BUY with higher confidence than S1.
  decision s2_buy = analyzer.analyze(
      stock::AAPL, make_state_with_price(93.1, 10000.0, false));
  EXPECT_EQ(s2_buy.act, action::BUY);
  EXPECT_DOUBLE_EQ(s2_buy.confidence, 1.0);
  EXPECT_GT(s2_buy.confidence, s1_buy.confidence);

  // Below S2 (90.0) with insufficient funds -> NO_ACTION.
  decision s2_no_funds =
      analyzer.analyze(stock::AAPL, make_state_with_price(90.0, 10.0, false));
  EXPECT_EQ(s2_no_funds.act, action::NO_ACTION);
}

TEST(PivotPointsAnalyzerTest, ReturnsNoActionAwayFromSupportAndResistance) {
  pivot_points_analyzer analyzer;

  // Price at Pivot (105.0) is outside all support/resistance bands.
  decision at_pivot = analyzer.analyze(
      stock::AAPL, make_state_with_price(105.0, 10000.0, true));
  EXPECT_EQ(at_pivot.act, action::NO_ACTION);

  // Price between R1 (110) and R2 (117).
  decision mid_resistance = analyzer.analyze(
      stock::AAPL, make_state_with_price(113.5, 10000.0, true));
  EXPECT_EQ(mid_resistance.act, action::NO_ACTION);

  // Price between S2 (93) and S1 (98).
  decision mid_support = analyzer.analyze(
      stock::AAPL, make_state_with_price(95.5, 10000.0, false));
  EXPECT_EQ(mid_support.act, action::NO_ACTION);
}

TEST(PivotPointsAnalyzerTest, ReturnsNoActionOnFlatPreviousSession) {
  trading_state state{};
  state.available_funds = 10000.0;
  aggregations& symbol_aggregations = state.market[stock::AAPL];
  symbol_aggregations.daily_candles.push_back(make_candle(
      100.0, 100.0, 100.0, 100.0, 1000, make_utc(2026, 11, 19, 14, 30)));
  add_next_minute(
      symbol_aggregations,
      make_candle(
          100.0, 100.5, 99.5, 100.0, 500, make_utc(2026, 11, 20, 15, 0)));

  pivot_points_analyzer analyzer;
  decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(result.confidence, 0.0);
}

TEST(PivotPointsAnalyzerTest, RejectsInvalidOptions) {
  const double quiet_nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();

  EXPECT_THROW(
      pivot_points_analyzer({.proximity_threshold = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.proximity_threshold = -0.01}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.proximity_threshold = 1.0}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.proximity_threshold = 1.5}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.proximity_threshold = quiet_nan}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.proximity_threshold = infinity}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.proximity_threshold = -infinity}),
      std::invalid_argument);

  EXPECT_THROW(
      pivot_points_analyzer({.confidence_scaler = 0.0}), std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.confidence_scaler = -0.5}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.confidence_scaler = quiet_nan}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.confidence_scaler = infinity}),
      std::invalid_argument);

  EXPECT_THROW(
      pivot_points_analyzer({.major_confidence = 0.0}), std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.major_confidence = -0.2}), std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.major_confidence = 1.1}), std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.major_confidence = quiet_nan}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.major_confidence = infinity}),
      std::invalid_argument);

  EXPECT_THROW(
      pivot_points_analyzer({.minor_confidence = 0.0}), std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.minor_confidence = -0.2}), std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.minor_confidence = 1.1}), std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.minor_confidence = quiet_nan}),
      std::invalid_argument);
  EXPECT_THROW(
      pivot_points_analyzer({.minor_confidence = infinity}),
      std::invalid_argument);
}

TEST(PivotPointsAnalyzerTest, AppliesConfidenceScalerWithClamping) {
  pivot_points_analyzer scaled_down({.confidence_scaler = 0.5});
  decision r1_scaled = scaled_down.analyze(
      stock::AAPL, make_state_with_price(110.0, 10000.0, true));
  EXPECT_EQ(r1_scaled.act, action::SELL);
  EXPECT_DOUBLE_EQ(r1_scaled.confidence, 0.35);

  pivot_points_analyzer scaled_up({.confidence_scaler = 2.0});
  decision r1_clamped = scaled_up.analyze(
      stock::AAPL, make_state_with_price(110.0, 10000.0, true));
  EXPECT_EQ(r1_clamped.act, action::SELL);
  EXPECT_DOUBLE_EQ(r1_clamped.confidence, 1.0);
}

TEST(PivotPointsAnalyzerTest, AppliesCustomMajorAndMinorConfidence) {
  pivot_points_analyzer analyzer(
      {.confidence_scaler = 0.5,
       .major_confidence = 0.8,
       .minor_confidence = 0.4});

  decision r2_decision = analyzer.analyze(
      stock::AAPL, make_state_with_price(117.0, 10000.0, true));
  EXPECT_EQ(r2_decision.act, action::SELL);
  EXPECT_DOUBLE_EQ(r2_decision.confidence, 0.4);

  decision r1_decision = analyzer.analyze(
      stock::AAPL, make_state_with_price(110.0, 10000.0, false));
  EXPECT_EQ(r1_decision.act, action::HOLD);
  EXPECT_DOUBLE_EQ(r1_decision.confidence, 0.2);

  decision s2_decision = analyzer.analyze(
      stock::AAPL, make_state_with_price(93.0, 10000.0, false));
  EXPECT_EQ(s2_decision.act, action::BUY);
  EXPECT_DOUBLE_EQ(s2_decision.confidence, 0.4);

  decision s1_decision = analyzer.analyze(
      stock::AAPL, make_state_with_price(98.0, 10000.0, false));
  EXPECT_EQ(s1_decision.act, action::BUY);
  EXPECT_DOUBLE_EQ(s1_decision.confidence, 0.2);
}

TEST(PivotPointsAnalyzerTest, LoadsViaFactoryByNameAndProto) {
  // Factory by string name.
  std::unique_ptr<analyzer> by_name = load_analyzer("pivot_points");
  ASSERT_NE(by_name, nullptr);
  decision name_result = by_name->analyze(
      stock::AAPL, make_state_with_price(98.0, 10000.0, false));
  EXPECT_EQ(name_result.act, action::BUY);
  EXPECT_DOUBLE_EQ(name_result.confidence, 0.7);

  // Factory by proto with unset (default) fields.
  Analyzer default_proto;
  default_proto.mutable_pivot_points();
  std::unique_ptr<analyzer> by_default_proto = load_analyzer(default_proto);
  ASSERT_NE(by_default_proto, nullptr);
  decision default_proto_result = by_default_proto->analyze(
      stock::AAPL, make_state_with_price(110.0, 10000.0, false));
  EXPECT_EQ(default_proto_result.act, action::HOLD);
  EXPECT_DOUBLE_EQ(default_proto_result.confidence, 0.7);

  // Factory by proto with custom options.
  Analyzer custom_proto;
  custom_proto.mutable_pivot_points()->set_proximity_threshold(0.01);
  custom_proto.mutable_pivot_points()->set_confidence_scaler(0.5);
  custom_proto.mutable_pivot_points()->set_major_confidence(0.9);
  custom_proto.mutable_pivot_points()->set_minor_confidence(0.6);
  std::unique_ptr<analyzer> by_custom_proto = load_analyzer(custom_proto);
  ASSERT_NE(by_custom_proto, nullptr);
  // 109.2 is within 1% of R1=110 (108.9..111.1), outside default 0.2%.
  decision custom_minor_result = by_custom_proto->analyze(
      stock::AAPL, make_state_with_price(109.2, 10000.0, true));
  EXPECT_EQ(custom_minor_result.act, action::SELL);
  EXPECT_DOUBLE_EQ(custom_minor_result.confidence, 0.3);

  decision custom_major_result = by_custom_proto->analyze(
      stock::AAPL, make_state_with_price(117.0, 10000.0, true));
  EXPECT_EQ(custom_major_result.act, action::SELL);
  EXPECT_DOUBLE_EQ(custom_major_result.confidence, 0.45);

  // Invalid proto options throw std::invalid_argument.
  Analyzer invalid_threshold_proto;
  invalid_threshold_proto.mutable_pivot_points()->set_proximity_threshold(1.5);
  EXPECT_THROW(
      (void)load_analyzer(invalid_threshold_proto), std::invalid_argument);

  Analyzer invalid_major_proto;
  invalid_major_proto.mutable_pivot_points()->set_major_confidence(1.2);
  EXPECT_THROW((void)load_analyzer(invalid_major_proto), std::invalid_argument);

  Analyzer invalid_minor_proto;
  invalid_minor_proto.mutable_pivot_points()->set_minor_confidence(-0.1);
  EXPECT_THROW((void)load_analyzer(invalid_minor_proto), std::invalid_argument);
}

} // namespace
} // namespace howling
