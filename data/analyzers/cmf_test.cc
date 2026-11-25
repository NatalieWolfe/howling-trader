#include "data/analyzers/cmf.h"

#include <cmath>
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
#include "trading/trading_state.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

Candle make_candle(
    double open, double high, double low, double close, int64_t volume) {
  Candle candle;
  candle.set_open(open);
  candle.set_high(high);
  candle.set_low(low);
  candle.set_close(close);
  candle.set_volume(volume);
  return candle;
}

trading_state make_state(double available_funds, bool holding_position) {
  trading_state state{};
  state.initial_funds = available_funds;
  state.available_funds = available_funds;
  if (holding_position) {
    state.positions[stock::AAPL].push_back(
        {.symbol = stock::AAPL, .price = 100.0, .quantity = 10});
  }
  return state;
}

TEST(CmfAnalyzerTest, AllClosesAtHighsProducesPositiveOneAndBuy) {
  trading_state state = make_state(/*available_funds=*/1000.0, false);
  aggregations& symbol_aggregation = state.market[stock::AAPL];
  for (int i = 0; i < 21; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 110.0, 1000));
  }

  cmf_analyzer analyzer;
  const std::optional<double> cmf = analyzer.calculate_cmf(stock::AAPL, state);
  ASSERT_TRUE(cmf.has_value());
  EXPECT_DOUBLE_EQ(*cmf, 1.0);

  const decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::BUY);
  EXPECT_DOUBLE_EQ(result.confidence, 1.0);
}

TEST(CmfAnalyzerTest, AccumulationWithoutFundsProducesHold) {
  trading_state state = make_state(/*available_funds=*/10.0, false);
  aggregations& symbol_aggregation = state.market[stock::AAPL];
  for (int i = 0; i < 21; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 110.0, 1000));
  }

  cmf_analyzer analyzer;
  const decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::HOLD);
  EXPECT_DOUBLE_EQ(result.confidence, 1.0);
}

TEST(CmfAnalyzerTest, AllClosesAtLowsProducesNegativeOneAndSellOrHold) {
  trading_state held_state = make_state(/*available_funds=*/1000.0, true);
  aggregations& held_aggregation = held_state.market[stock::AAPL];
  for (int i = 0; i < 21; ++i) {
    add_next_minute(
        held_aggregation, make_candle(100.0, 110.0, 90.0, 90.0, 1000));
  }

  cmf_analyzer analyzer;
  const std::optional<double> cmf =
      analyzer.calculate_cmf(stock::AAPL, held_state);
  ASSERT_TRUE(cmf.has_value());
  EXPECT_DOUBLE_EQ(*cmf, -1.0);

  const decision sell_result = analyzer.analyze(stock::AAPL, held_state);
  EXPECT_EQ(sell_result.act, action::SELL);
  EXPECT_DOUBLE_EQ(sell_result.confidence, 1.0);

  held_state.positions.clear();
  const decision hold_result = analyzer.analyze(stock::AAPL, held_state);
  EXPECT_EQ(hold_result.act, action::HOLD);
  EXPECT_DOUBLE_EQ(hold_result.confidence, 1.0);
}

TEST(CmfAnalyzerTest, ClosesAtMidpointProducesZeroAndNoAction) {
  trading_state state = make_state(/*available_funds=*/1000.0, true);
  aggregations& symbol_aggregation = state.market[stock::AAPL];
  for (int i = 0; i < 21; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 100.0, 1000));
  }

  cmf_analyzer analyzer;
  const std::optional<double> cmf = analyzer.calculate_cmf(stock::AAPL, state);
  ASSERT_TRUE(cmf.has_value());
  EXPECT_DOUBLE_EQ(*cmf, 0.0);

  const decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(result.confidence, 0.0);
}

TEST(CmfAnalyzerTest, VolumeWeighted21PeriodHandCalculation) {
  trading_state state = make_state(/*available_funds=*/1000.0, false);
  aggregations& symbol_aggregation = state.market[stock::AAPL];

  // 14 bullish bars: high=110, low=90, close=105 -> MFM = +0.5, vol=200 each.
  //   MFV sum = 14 * 0.5 * 200 = +1400, volume = 2800.
  // 7 bearish bars: high=110, low=90, close=95 -> MFM = -0.5, vol=100 each.
  //   MFV sum = 7 * (-0.5) * 100 = -350, volume = 700.
  // Total MFV = 1050, total volume = 3500 -> CMF = 1050 / 3500 = 0.3.
  for (int i = 0; i < 14; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 105.0, 200));
  }
  for (int i = 0; i < 7; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 95.0, 100));
  }

  cmf_analyzer analyzer;
  const std::optional<double> cmf = analyzer.calculate_cmf(stock::AAPL, state);
  ASSERT_TRUE(cmf.has_value());
  EXPECT_NEAR(*cmf, 0.3, 1e-12);

  // Confidence = min(1.0, 0.3 * 2.0) = 0.6.
  const decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::BUY);
  EXPECT_NEAR(result.confidence, 0.6, 1e-12);
}

TEST(CmfAnalyzerTest, FlatBarsDoNotProduceNanOrInf) {
  trading_state state = make_state(/*available_funds=*/1000.0, false);
  aggregations& symbol_aggregation = state.market[stock::AAPL];
  for (int i = 0; i < 21; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 100.0, 100.0, 100.0, 500));
  }

  cmf_analyzer analyzer;
  const std::optional<double> cmf = analyzer.calculate_cmf(stock::AAPL, state);
  ASSERT_TRUE(cmf.has_value());
  EXPECT_FALSE(std::isnan(*cmf));
  EXPECT_FALSE(std::isinf(*cmf));
  EXPECT_DOUBLE_EQ(*cmf, 0.0);

  const decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
}

TEST(CmfAnalyzerTest, ZeroTotalVolumeReturnsNulloptAndNoAction) {
  trading_state state = make_state(/*available_funds=*/1000.0, true);
  aggregations& symbol_aggregation = state.market[stock::AAPL];
  for (int i = 0; i < 21; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 110.0, 0));
  }

  cmf_analyzer analyzer;
  EXPECT_FALSE(analyzer.calculate_cmf(stock::AAPL, state).has_value());
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
}

TEST(CmfAnalyzerTest, MissingSymbolAndInsufficientHistoryReturnNoAction) {
  trading_state state = make_state(/*available_funds=*/1000.0, true);
  cmf_analyzer analyzer;
  EXPECT_FALSE(analyzer.calculate_cmf(stock::AAPL, state).has_value());
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  aggregations& symbol_aggregation = state.market[stock::AAPL];
  for (int i = 0; i < 20; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 110.0, 1000));
  }
  EXPECT_FALSE(analyzer.calculate_cmf(stock::AAPL, state).has_value());
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
}

TEST(CmfAnalyzerTest, MultiMinuteStridedSamplingAvoidsVolumeDoubleCounting) {
  trading_state state = make_state(/*available_funds=*/1000.0, false);
  aggregations& symbol_aggregation = state.market[stock::AAPL];

  // Build 2 non-overlapping 5-minute windows via add_next_minute (10 minutes):
  // Block 0 (minutes 0..4): bullish close at high (110), volume=100 per min
  //   -> 5m window: high=110, low=90, close=110, vol=500, MFM=+1.0.
  // Block 1 (minutes 5..9): bearish close at low (90), volume=300 per min
  //   -> 5m window: high=110, low=90, close=90, vol=1500, MFM=-1.0.
  // Non-overlapping CMF = (500 * 1.0 + 1500 * -1.0) / 2000 = -0.5.
  for (int i = 0; i < 5; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 110.0, 100));
  }
  for (int i = 0; i < 4; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 90.0, 300));
  }

  cmf_analyzer analyzer(
      &aggregations::five_minute,
      cmf_analyzer::options{
          .lookback_periods = 2, .enable_divergence_check = false});

  // With only 9 minutes (< 2 * 5), history is insufficient.
  EXPECT_FALSE(analyzer.calculate_cmf(stock::AAPL, state).has_value());
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Add the 10th minute to complete the second non-overlapping 5-minute bar.
  add_next_minute(
      symbol_aggregation, make_candle(100.0, 110.0, 90.0, 90.0, 300));
  const std::optional<double> cmf = analyzer.calculate_cmf(stock::AAPL, state);
  ASSERT_TRUE(cmf.has_value());
  EXPECT_NEAR(*cmf, -0.5, 1e-12);
}

TEST(CmfAnalyzerTest, BearishDivergenceTriggersSellOrHoldEvenWhenCmfPositive) {
  trading_state state = make_state(/*available_funds=*/1000.0, true);
  aggregations& symbol_aggregation = state.market[stock::AAPL];

  // Build 21 rising bars closing at highs -> prior CMF = +1.0.
  for (int i = 0; i < 21; ++i) {
    const double base = 100.0 + static_cast<double>(i);
    add_next_minute(
        symbol_aggregation,
        make_candle(base, base + 10.0, base, base + 10.0, 100));
  }
  // Bar 22 makes a new price high (135 > 130) but closes at its low (MFM=-1),
  // pulling CMF down from +1.0 to (20*100 - 1*100)/2100 = +19/21 (~+0.905).
  add_next_minute(
      symbol_aggregation, make_candle(130.0, 135.0, 125.0, 125.0, 100));

  cmf_analyzer divergence_enabled;
  const std::optional<double> cmf =
      divergence_enabled.calculate_cmf(stock::AAPL, state);
  ASSERT_TRUE(cmf.has_value());
  EXPECT_GT(*cmf, 0.05);

  const decision sell_decision = divergence_enabled.analyze(stock::AAPL, state);
  EXPECT_EQ(sell_decision.act, action::SELL);
  EXPECT_GT(sell_decision.confidence, 0.0);
  EXPECT_LE(sell_decision.confidence, 1.0);

  // Without a held position, bearish divergence produces HOLD.
  state.positions.clear();
  const decision hold_decision = divergence_enabled.analyze(stock::AAPL, state);
  EXPECT_EQ(hold_decision.act, action::HOLD);
  EXPECT_GT(hold_decision.confidence, 0.0);

  // With divergence check disabled, positive CMF produces BUY.
  cmf_analyzer divergence_disabled(
      &aggregations::one_minute,
      cmf_analyzer::options{.enable_divergence_check = false});
  const decision buy_decision = divergence_disabled.analyze(stock::AAPL, state);
  EXPECT_EQ(buy_decision.act, action::BUY);
}

TEST(CmfAnalyzerTest, InvalidConstructorOptionsThrowInvalidArgument) {
  EXPECT_THROW(cmf_analyzer(nullptr), std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{.lookback_periods = 0}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{.lookback_periods = -5}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute, cmf_analyzer::options{.threshold = -0.01}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute, cmf_analyzer::options{.threshold = 1.0}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{
              .threshold = std::numeric_limits<double>::quiet_NaN()}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{
              .threshold = std::numeric_limits<double>::infinity()}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{.confidence_scaler = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{.confidence_scaler = -2.0}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{
              .confidence_scaler = std::numeric_limits<double>::infinity()}),
      std::invalid_argument);
  EXPECT_THROW(
      cmf_analyzer(
          &aggregations::one_minute,
          cmf_analyzer::options{
              .confidence_scaler = std::numeric_limits<double>::quiet_NaN()}),
      std::invalid_argument);
}

TEST(CmfAnalyzerTest, LoadAnalyzerFactorySupportsStringNamesAndProto) {
  trading_state state = make_state(/*available_funds=*/1000.0, false);
  aggregations& symbol_aggregation = state.market[stock::AAPL];
  for (int i = 0; i < 21; ++i) {
    add_next_minute(
        symbol_aggregation, make_candle(100.0, 110.0, 90.0, 110.0, 1000));
  }

  std::unique_ptr<analyzer> cmf_default = load_analyzer("cmf");
  std::unique_ptr<analyzer> cmf1 = load_analyzer("cmf1");
  std::unique_ptr<analyzer> cmf5 = load_analyzer("cmf5");
  std::unique_ptr<analyzer> cmf20 = load_analyzer("cmf20");
  ASSERT_NE(cmf_default, nullptr);
  ASSERT_NE(cmf1, nullptr);
  ASSERT_NE(cmf5, nullptr);
  ASSERT_NE(cmf20, nullptr);

  EXPECT_EQ(cmf_default->analyze(stock::AAPL, state).act, action::BUY);
  EXPECT_EQ(cmf1->analyze(stock::AAPL, state).act, action::BUY);
  // 21 one-minute candles are insufficient for 21 five-minute or 20-minute
  // windows.
  EXPECT_EQ(cmf5->analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_EQ(cmf20->analyze(stock::AAPL, state).act, action::NO_ACTION);

  Analyzer proto_config;
  Analyzer::Cmf* cmf_proto = proto_config.mutable_cmf();
  cmf_proto->set_period(Analyzer::ONE);
  cmf_proto->set_lookback_periods(21);
  cmf_proto->set_threshold(0.1);
  cmf_proto->set_confidence_scaler(1.5);
  cmf_proto->set_enable_divergence_check(true);

  std::unique_ptr<analyzer> from_proto = load_analyzer(proto_config);
  ASSERT_NE(from_proto, nullptr);
  const decision proto_decision = from_proto->analyze(stock::AAPL, state);
  EXPECT_EQ(proto_decision.act, action::BUY);
  EXPECT_DOUBLE_EQ(proto_decision.confidence, 1.0);

  Analyzer unspecified_period_config;
  unspecified_period_config.mutable_cmf()->set_period(
      Analyzer::PERIOD_UNSPECIFIED);
  EXPECT_THROW(
      (void)load_analyzer(unspecified_period_config), std::invalid_argument);
}

} // namespace
} // namespace howling
