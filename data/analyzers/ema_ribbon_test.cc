#include "data/analyzers/ema_ribbon.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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

Candle make_candle(double close_price) {
  Candle candle;
  candle.set_open(close_price);
  candle.set_high(close_price + 0.5);
  candle.set_low(std::max(0.01, close_price - 0.5));
  candle.set_close(close_price);
  candle.set_volume(100);
  return candle;
}

system_clock::time_point make_market_time(int minute_offset) {
  // 14:30 UTC in EST is 09:30 ET.
  return sys_days{year{2026} / month{11} / day{20}} + hours{14} +
      minutes{30 + minute_offset};
}

void push_price(
    trading_state& state,
    stock::Symbol symbol,
    double close_price,
    int minute_offset) {
  state.time_now = make_market_time(minute_offset);
  add_next_minute(state.market[symbol], make_candle(close_price));
}

TEST(EmaRibbonAnalyzerTest, ValidatesConstructorOptions) {
  constexpr double infinity = std::numeric_limits<double>::infinity();
  constexpr double quiet_nan = std::numeric_limits<double>::quiet_NaN();

  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.periods = {}}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.periods = {9}}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.periods = {0, 9}}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.periods = {-5, 21}}),
      std::invalid_argument);
  try {
    ema_ribbon_analyzer(
        &aggregations::one_minute,
        ema_ribbon_analyzer::options{.periods = {9, 21, 9}});
    FAIL() << "Expected std::invalid_argument for duplicate periods.";
  } catch (const std::invalid_argument& err) {
    EXPECT_NE(std::string(err.what()).find("9"), std::string::npos);
  }
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.min_spread_ratio = -0.001}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.min_spread_ratio = quiet_nan}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.min_spread_ratio = infinity}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.min_spread_ratio = -infinity}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.confidence_scaler = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.confidence_scaler = -10.0}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.confidence_scaler = quiet_nan}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.confidence_scaler = infinity}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(
          &aggregations::one_minute,
          ema_ribbon_analyzer::options{.confidence_scaler = -infinity}),
      std::invalid_argument);
  EXPECT_THROW(
      ema_ribbon_analyzer(nullptr, ema_ribbon_analyzer::options{}),
      std::invalid_argument);
}

TEST(EmaRibbonAnalyzerTest, SeedsWithSmaAndUpdatesExponentially) {
  ema_ribbon_analyzer analyzer(
      &aggregations::one_minute,
      ema_ribbon_analyzer::options{.periods = {2, 3}});
  trading_state state{};
  state.available_funds = 10000.0;

  // Samples: 10, 20, 30.
  // Period 2: seeded at bar 2 with (10 + 20) / 2 = 15.0;
  //   bar 3 with multiplier 2/3 -> 30 * (2/3) + 15 * (1/3) = 25.0.
  // Period 3: seeded at bar 3 with (10 + 20 + 30) / 3 = 20.0.
  push_price(state, stock::AAPL, 10.0, 0);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  push_price(state, stock::AAPL, 20.0, 1);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  push_price(state, stock::AAPL, 30.0, 2);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::BUY);

  std::optional<std::vector<double>> emas = analyzer.current_emas(stock::AAPL);
  ASSERT_TRUE(emas.has_value());
  ASSERT_EQ(emas->size(), 2u);
  EXPECT_DOUBLE_EQ((*emas)[0], 25.0);
  EXPECT_DOUBLE_EQ((*emas)[1], 20.0);

  // Sample 4: 40.
  // Period 2: 40 * (2/3) + 25 * (1/3) = 35.0.
  // Period 3: multiplier 2/4 = 0.5 -> 40 * 0.5 + 20 * 0.5 = 30.0.
  push_price(state, stock::AAPL, 40.0, 3);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::BUY);
  emas = analyzer.current_emas(stock::AAPL);
  ASSERT_TRUE(emas.has_value());
  EXPECT_DOUBLE_EQ((*emas)[0], 35.0);
  EXPECT_DOUBLE_EQ((*emas)[1], 30.0);
}

TEST(EmaRibbonAnalyzerTest, MissingSymbolAndWarmupReturnNoAction) {
  ema_ribbon_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  // Missing symbol returns NO_ACTION.
  decision missing = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(missing.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(missing.confidence, 0.0);
  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::UNKNOWN);
  EXPECT_FALSE(analyzer.current_emas(stock::AAPL).has_value());

  // Default periods are {9, 21, 50}, so bars 1..49 stay in warmup.
  for (int i = 0; i < 49; ++i) {
    push_price(state, stock::AAPL, 100.0 + i * 0.5, i);
    decision result = analyzer.analyze(stock::AAPL, state);
    EXPECT_EQ(result.act, action::NO_ACTION);
    EXPECT_EQ(
        analyzer.current_trend(stock::AAPL),
        ema_ribbon_analyzer::trend::UNKNOWN);
    EXPECT_FALSE(analyzer.current_emas(stock::AAPL).has_value());
  }

  // Bar 50 completes warmup.
  push_price(state, stock::AAPL, 100.0 + 49 * 0.5, 49);
  decision ready = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(ready.act, action::BUY);
  EXPECT_GT(ready.confidence, 0.0);
  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::BULLISH);
  std::optional<std::vector<double>> emas = analyzer.current_emas(stock::AAPL);
  ASSERT_TRUE(emas.has_value());
  ASSERT_EQ(emas->size(), 3u);
  EXPECT_GT((*emas)[0], (*emas)[1]);
  EXPECT_GT((*emas)[1], (*emas)[2]);
}

TEST(EmaRibbonAnalyzerTest, SustainedUptrendEmitsBuyOrHoldBasedOnFunds) {
  // Pass unsorted periods to verify ascending sort in constructor.
  ema_ribbon_analyzer analyzer(
      &aggregations::one_minute,
      ema_ribbon_analyzer::options{
          .periods = {50, 9, 21},
          .min_spread_ratio = 0.001,
          .confidence_scaler = 100.0,
      });
  trading_state state{};
  state.available_funds = 10000.0;

  decision last_result = NO_ACTION;
  for (int i = 0; i < 60; ++i) {
    push_price(state, stock::AAPL, 100.0 + i * 1.0, i);
    last_result = analyzer.analyze(stock::AAPL, state);
  }

  EXPECT_EQ(last_result.act, action::BUY);
  EXPECT_GT(last_result.confidence, 0.0);
  EXPECT_LE(last_result.confidence, 1.0);
  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::BULLISH);

  // Without enough available funds, bullish alignment emits HOLD.
  state.available_funds = 10.0;
  push_price(state, stock::AAPL, 161.0, 60);
  decision no_funds = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(no_funds.act, action::HOLD);
  EXPECT_GT(no_funds.confidence, 0.0);
  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::BULLISH);
}

TEST(EmaRibbonAnalyzerTest, SustainedDowntrendEmitsSellOrHoldBasedOnPositions) {
  ema_ribbon_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  decision last_result = NO_ACTION;
  for (int i = 0; i < 60; ++i) {
    push_price(state, stock::AAPL, 200.0 - i * 1.0, i);
    last_result = analyzer.analyze(stock::AAPL, state);
  }

  // Without held positions, bearish alignment emits HOLD.
  EXPECT_EQ(last_result.act, action::HOLD);
  EXPECT_GT(last_result.confidence, 0.0);
  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::BEARISH);

  std::optional<std::vector<double>> emas = analyzer.current_emas(stock::AAPL);
  ASSERT_TRUE(emas.has_value());
  ASSERT_EQ(emas->size(), 3u);
  EXPECT_LT((*emas)[0], (*emas)[1]);
  EXPECT_LT((*emas)[1], (*emas)[2]);

  // With a held position, bearish alignment emits SELL.
  state.positions[stock::AAPL].push_back(
      {.symbol = stock::AAPL, .price = 180.0, .quantity = 10});
  push_price(state, stock::AAPL, 139.0, 60);
  decision can_sell_result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(can_sell_result.act, action::SELL);
  EXPECT_GT(can_sell_result.confidence, 0.0);
  EXPECT_LE(can_sell_result.confidence, 1.0);
}

TEST(EmaRibbonAnalyzerTest, SidewaysOscillatingSeriesIsTangledAndHolds) {
  ema_ribbon_analyzer analyzer(
      &aggregations::one_minute,
      ema_ribbon_analyzer::options{
          .periods = {9, 21, 50},
          .min_spread_ratio = 0.005,
          .confidence_scaler = 100.0,
      });
  trading_state state{};
  state.available_funds = 10000.0;
  state.positions[stock::AAPL].push_back(
      {.symbol = stock::AAPL, .price = 100.0, .quantity = 5});

  for (int i = 0; i < 80; ++i) {
    const double price = 100.0 + ((i % 2 == 0) ? 0.1 : -0.1);
    push_price(state, stock::AAPL, price, i);
    decision result = analyzer.analyze(stock::AAPL, state);
    if (i >= 49) {
      EXPECT_EQ(result.act, action::HOLD);
      EXPECT_EQ(
          analyzer.current_trend(stock::AAPL),
          ema_ribbon_analyzer::trend::TANGLED);
      EXPECT_GE(result.confidence, 0.0);
      EXPECT_LE(result.confidence, 1.0);
    }
  }
}

TEST(EmaRibbonAnalyzerTest, CustomFourPeriodRibbonWarmsUpAtBar200) {
  ema_ribbon_analyzer analyzer(
      &aggregations::one_minute,
      ema_ribbon_analyzer::options{.periods = {9, 21, 50, 200}});
  trading_state state{};
  state.available_funds = 10000.0;

  for (int i = 0; i < 199; ++i) {
    push_price(state, stock::AAPL, 100.0 + i * 0.25, i);
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  }
  EXPECT_FALSE(analyzer.current_emas(stock::AAPL).has_value());

  push_price(state, stock::AAPL, 100.0 + 199 * 0.25, 199);
  decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::BUY);
  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::BULLISH);

  std::optional<std::vector<double>> emas = analyzer.current_emas(stock::AAPL);
  ASSERT_TRUE(emas.has_value());
  ASSERT_EQ(emas->size(), 4u);
  EXPECT_GT((*emas)[0], (*emas)[1]);
  EXPECT_GT((*emas)[1], (*emas)[2]);
  EXPECT_GT((*emas)[2], (*emas)[3]);
}

TEST(EmaRibbonAnalyzerTest, IsolatesStateAcrossMultipleSymbols) {
  ema_ribbon_analyzer analyzer(
      &aggregations::one_minute,
      ema_ribbon_analyzer::options{.periods = {3, 6, 10}});
  trading_state state{};
  state.available_funds = 10000.0;
  state.positions[stock::NVDA].push_back(
      {.symbol = stock::NVDA, .price = 200.0, .quantity = 5});

  for (int i = 0; i < 15; ++i) {
    push_price(state, stock::AAPL, 100.0 + i * 2.0, i);
    push_price(state, stock::NVDA, 200.0 - i * 2.0, i);
    decision aapl_decision = analyzer.analyze(stock::AAPL, state);
    decision nvda_decision = analyzer.analyze(stock::NVDA, state);

    if (i >= 9) {
      EXPECT_EQ(aapl_decision.act, action::BUY);
      EXPECT_EQ(nvda_decision.act, action::SELL);
    }
  }

  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::BULLISH);
  EXPECT_EQ(
      analyzer.current_trend(stock::NVDA), ema_ribbon_analyzer::trend::BEARISH);
}

TEST(EmaRibbonAnalyzerTest, FiveMinutePeriodStepsOnWindowBoundaries) {
  ema_ribbon_analyzer analyzer(
      &aggregations::five_minute,
      ema_ribbon_analyzer::options{.periods = {2, 4}});
  trading_state state{};
  state.available_funds = 10000.0;

  // Minutes 1..20 (indices 0..19 -> market minutes 31..50).
  // Boundaries at market minutes 35, 40, 45, 50 -> 4 five-minute bars.
  decision last_boundary_result = NO_ACTION;
  for (int i = 1; i <= 20; ++i) {
    push_price(state, stock::AAPL, 100.0 + i * 1.0, i);
    decision result = analyzer.analyze(stock::AAPL, state);
    if (i % 5 != 0) {
      EXPECT_EQ(result.act, action::NO_ACTION);
    } else {
      last_boundary_result = result;
      if (i < 20) { EXPECT_EQ(result.act, action::NO_ACTION); }
    }
  }

  EXPECT_EQ(last_boundary_result.act, action::BUY);
  EXPECT_EQ(
      analyzer.current_trend(stock::AAPL), ema_ribbon_analyzer::trend::BULLISH);
}

TEST(EmaRibbonAnalyzerTest, LoadAnalyzerSupportsNamesAndProtoConfig) {
  ASSERT_NE(load_analyzer("ema_ribbon"), nullptr);
  ASSERT_NE(load_analyzer("ema_ribbon1"), nullptr);
  ASSERT_NE(load_analyzer("ema_ribbon5"), nullptr);
  ASSERT_NE(load_analyzer("ema_ribbon20"), nullptr);

  Analyzer proto_config;
  Analyzer::EmaRibbon* ribbon = proto_config.mutable_ema_ribbon();
  ribbon->add_periods(3);
  ribbon->add_periods(6);
  ribbon->add_periods(10);
  ribbon->set_min_spread_ratio(0.001);
  ribbon->set_confidence_scaler(50.0);
  ribbon->set_candle_period(Analyzer::ONE);

  std::unique_ptr<analyzer> by_proto = load_analyzer(proto_config);
  ASSERT_NE(by_proto, nullptr);

  trading_state state{};
  state.available_funds = 10000.0;
  decision result = NO_ACTION;
  for (int i = 0; i < 12; ++i) {
    push_price(state, stock::AAPL, 100.0 + i * 1.5, i);
    result = by_proto->analyze(stock::AAPL, state);
  }
  EXPECT_EQ(result.act, action::BUY);
  EXPECT_GT(result.confidence, 0.0);

  // Proto with only candle_period set uses default options ({9, 21, 50}).
  Analyzer default_proto;
  default_proto.mutable_ema_ribbon()->set_candle_period(Analyzer::ONE);
  std::unique_ptr<analyzer> default_loaded = load_analyzer(default_proto);
  ASSERT_NE(default_loaded, nullptr);
  EXPECT_EQ(default_loaded->analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Proto with unspecified candle_period throws std::invalid_argument.
  Analyzer missing_period_proto;
  missing_period_proto.mutable_ema_ribbon();
  EXPECT_THROW(load_analyzer(missing_period_proto), std::invalid_argument);
}

} // namespace
} // namespace howling
