#include "data/analyzers/rsi.h"

#include <chrono>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
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

Candle make_candle(double close) {
  Candle candle;
  candle.set_open(close);
  candle.set_high(close + 0.5);
  candle.set_low(close - 0.5);
  candle.set_close(close);
  candle.set_volume(100);
  return candle;
}

system_clock::time_point make_market_utc(int minute_offset) {
  // 14:30 UTC in EST is 09:30 ET.
  return sys_days{year{2026} / month{11} / day{20}} + hours{14} +
      minutes{30 + minute_offset};
}

void push_close(
    trading_state& state,
    stock::Symbol symbol,
    double close,
    int minute_offset) {
  state.time_now = make_market_utc(minute_offset);
  add_next_minute(state.market[symbol], make_candle(close));
}

TEST(RsiAnalyzerTest, RejectsInvalidConstructorArguments) {
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute, rsi_analyzer::options{.period = 0}),
      std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute, rsi_analyzer::options{.period = -14}),
      std::invalid_argument);
  EXPECT_THROW(rsi_analyzer(nullptr), std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(nullptr, rsi_analyzer::options{}), std::invalid_argument);

  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute,
          rsi_analyzer::options{.oversold_threshold = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute,
          rsi_analyzer::options{.oversold_threshold = -10.0}),
      std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute,
          rsi_analyzer::options{.overbought_threshold = 100.0}),
      std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute,
          rsi_analyzer::options{.overbought_threshold = 110.0}),
      std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute,
          rsi_analyzer::options{
              .oversold_threshold = 70.0, .overbought_threshold = 70.0}),
      std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute,
          rsi_analyzer::options{
              .oversold_threshold = 75.0, .overbought_threshold = 25.0}),
      std::invalid_argument);
  EXPECT_THROW(
      rsi_analyzer(
          &aggregations::one_minute,
          rsi_analyzer::options{
              .oversold_threshold = std::numeric_limits<double>::quiet_NaN()}),
      std::invalid_argument);
}

TEST(RsiAnalyzerTest, MissingSymbolOrEmptySeriesReturnsNoAction) {
  rsi_analyzer analyzer;
  trading_state state{};

  decision result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_DOUBLE_EQ(result.confidence, 0.0);
  EXPECT_FALSE(analyzer.current_rsi(stock::AAPL).has_value());

  state.market[stock::AAPL];
  result = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(result.act, action::NO_ACTION);
  EXPECT_FALSE(analyzer.current_rsi(stock::AAPL).has_value());
}

TEST(RsiAnalyzerTest, WarmupRequiresPeriodPlusOneCandles) {
  rsi_analyzer analyzer(
      &aggregations::one_minute, rsi_analyzer::options{.period = 5});
  trading_state state{};
  state.available_funds = 10000.0;

  for (int i = 0; i < 5; ++i) {
    push_close(state, stock::AAPL, 100.0 - i, i);
    decision result = analyzer.analyze(stock::AAPL, state);
    EXPECT_EQ(result.act, action::NO_ACTION);
    EXPECT_FALSE(analyzer.current_rsi(stock::AAPL).has_value());
  }

  // Sixth candle completes 5 price deltas and produces the initial RSI.
  push_close(state, stock::AAPL, 95.0, 5);
  decision result = analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 0.0);
  EXPECT_EQ(result.act, action::BUY);
  EXPECT_DOUBLE_EQ(result.confidence, 1.0);
}

TEST(RsiAnalyzerTest, MonotonicUptrendYieldsRsi100AndSellOrHold) {
  rsi_analyzer analyzer(
      &aggregations::one_minute, rsi_analyzer::options{.period = 3});
  trading_state state{};
  state.available_funds = 10000.0;

  for (int i = 0; i < 4; ++i) {
    push_close(state, stock::AAPL, 100.0 + i, i);
    decision result = analyzer.analyze(stock::AAPL, state);
    if (i < 3) {
      EXPECT_EQ(result.act, action::NO_ACTION);
    } else {
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 100.0);
      // No positions held -> HOLD instead of SELL.
      EXPECT_EQ(result.act, action::HOLD);
      EXPECT_DOUBLE_EQ(result.confidence, 1.0);
    }
  }

  // Hold a position -> next overbought reading triggers SELL.
  state.positions[stock::AAPL].push_back(
      {.symbol = stock::AAPL, .price = 100.0, .quantity = 1});
  push_close(state, stock::AAPL, 104.0, 4);
  decision sell_result = analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 100.0);
  EXPECT_EQ(sell_result.act, action::SELL);
  EXPECT_DOUBLE_EQ(sell_result.confidence, 1.0);
}

TEST(RsiAnalyzerTest, MonotonicDowntrendYieldsRsi0AndBuyOrHold) {
  rsi_analyzer analyzer(
      &aggregations::one_minute, rsi_analyzer::options{.period = 3});
  trading_state state{};
  state.available_funds = 10000.0;

  for (int i = 0; i < 4; ++i) {
    push_close(state, stock::AAPL, 100.0 - i, i);
    decision result = analyzer.analyze(stock::AAPL, state);
    if (i < 3) {
      EXPECT_EQ(result.act, action::NO_ACTION);
    } else {
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 0.0);
      EXPECT_EQ(result.act, action::BUY);
      EXPECT_DOUBLE_EQ(result.confidence, 1.0);
    }
  }

  // Insufficient funds -> next oversold reading triggers HOLD.
  state.available_funds = 0.0;
  push_close(state, stock::AAPL, 96.0, 4);
  decision hold_result = analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 0.0);
  EXPECT_EQ(hold_result.act, action::HOLD);
  EXPECT_DOUBLE_EQ(hold_result.confidence, 1.0);
}

TEST(RsiAnalyzerTest, FlatPriceSeriesYieldsRsi50AndNoAction) {
  rsi_analyzer analyzer(
      &aggregations::one_minute, rsi_analyzer::options{.period = 4});
  trading_state state{};
  state.available_funds = 10000.0;

  for (int i = 0; i < 8; ++i) {
    push_close(state, stock::AAPL, 50.0, i);
    decision result = analyzer.analyze(stock::AAPL, state);
    EXPECT_EQ(result.act, action::NO_ACTION);
    EXPECT_DOUBLE_EQ(result.confidence, 0.0);
  }

  ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
  EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 50.0);
}

TEST(RsiAnalyzerTest, MatchesHandVerified14PeriodWilderSequence) {
  rsi_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;
  state.positions[stock::AAPL].push_back(
      {.symbol = stock::AAPL, .price = 100.0, .quantity = 1});

  // First 14 deltas alternate +4 and -2 (7 gains = 28, 7 losses = 14):
  //   Candle 14 (seed): avg_gain = 2.0, avg_loss = 1.0 -> RSI = 200 / 3.
  //   Candle 15 (+16):  avg_gain = 3.0, avg_loss = 13/14 -> RSI = 840 / 11.
  //   Candle 16 (-15):  avg_gain = 39/14, avg_loss = 379/196 -> RSI = 2184/37.
  //   Candle 17 (0):    both scale by 13/14 -> RSI unchanged = 2184/37.
  const std::vector<double> closes = {
      100.0,
      104.0,
      102.0,
      106.0,
      104.0,
      108.0,
      106.0,
      110.0,
      108.0,
      112.0,
      110.0,
      114.0,
      112.0,
      116.0,
      114.0,
      130.0,
      115.0,
      115.0,
  };

  for (size_t i = 0; i < closes.size(); ++i) {
    push_close(state, stock::AAPL, closes[i], static_cast<int>(i));
    decision result = analyzer.analyze(stock::AAPL, state);

    if (i < 14) {
      EXPECT_EQ(result.act, action::NO_ACTION);
      EXPECT_FALSE(analyzer.current_rsi(stock::AAPL).has_value());
    } else if (i == 14) {
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      EXPECT_NEAR(*analyzer.current_rsi(stock::AAPL), 200.0 / 3.0, 1e-6);
      EXPECT_EQ(result.act, action::NO_ACTION);
    } else if (i == 15) {
      const double expected_rsi = 840.0 / 11.0;
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      EXPECT_NEAR(*analyzer.current_rsi(stock::AAPL), expected_rsi, 1e-6);
      EXPECT_EQ(result.act, action::SELL);
      EXPECT_NEAR(result.confidence, (expected_rsi - 70.0) / 30.0, 1e-6);
    } else if (i == 16 || i == 17) {
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      EXPECT_NEAR(*analyzer.current_rsi(stock::AAPL), 2184.0 / 37.0, 1e-6);
      EXPECT_EQ(result.act, action::NO_ACTION);
    }
  }
}

TEST(RsiAnalyzerTest, MultiMinuteWindowStepsOnlyOnFullWindowBoundaries) {
  rsi_analyzer analyzer(
      &aggregations::five_minute, rsi_analyzer::options{.period = 2});
  trading_state state{};
  state.available_funds = 10000.0;

  // Push 16 minutes starting at 09:31 ET (minute_offset = 1 .. 16).
  // Full 5-minute boundaries occur at 09:35 (offset 5), 09:40 (offset 10),
  // and 09:45 (offset 15).
  for (int minute = 1; minute <= 16; ++minute) {
    // Monotonically decreasing closes so RSI = 0.0 once warmed up.
    push_close(state, stock::AAPL, 200.0 - minute, minute);
    decision result = analyzer.analyze(stock::AAPL, state);

    if (minute < 15) {
      // Minutes 1..4 are incomplete windows; 5 & 10 are first 2 full windows
      // (only 1 delta); non-multiples of 5 are skipped.
      EXPECT_EQ(result.act, action::NO_ACTION);
      EXPECT_FALSE(analyzer.current_rsi(stock::AAPL).has_value());
    } else if (minute == 15) {
      // Third full 5-minute boundary completes period=2 deltas.
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 0.0);
      EXPECT_EQ(result.act, action::BUY);
      EXPECT_DOUBLE_EQ(result.confidence, 1.0);
    } else {
      // Minute 16 (09:46 ET) is a non-step minute -> NO_ACTION.
      EXPECT_EQ(result.act, action::NO_ACTION);
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 0.0);
    }
  }
}

TEST(RsiAnalyzerTest, MultiSymbolStateIsIsolated) {
  rsi_analyzer analyzer(
      &aggregations::one_minute, rsi_analyzer::options{.period = 3});
  trading_state state{};
  state.available_funds = 10000.0;
  state.positions[stock::AAPL].push_back(
      {.symbol = stock::AAPL, .price = 100.0, .quantity = 1});

  for (int i = 0; i < 4; ++i) {
    push_close(state, stock::AAPL, 100.0 + i, i);
    push_close(state, stock::NVDA, 100.0 - i, i);
    decision aapl_decision = analyzer.analyze(stock::AAPL, state);
    decision nvda_decision = analyzer.analyze(stock::NVDA, state);

    if (i == 3) {
      ASSERT_TRUE(analyzer.current_rsi(stock::AAPL).has_value());
      ASSERT_TRUE(analyzer.current_rsi(stock::NVDA).has_value());
      EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::AAPL), 100.0);
      EXPECT_DOUBLE_EQ(*analyzer.current_rsi(stock::NVDA), 0.0);
      EXPECT_EQ(aapl_decision.act, action::SELL);
      EXPECT_EQ(nvda_decision.act, action::BUY);
    }
  }
}

TEST(RsiAnalyzerTest, LoadAnalyzerSupportsStringsAndProto) {
  for (const std::string_view name : {"rsi", "rsi1", "rsi5", "rsi20"}) {
    std::unique_ptr<analyzer> loaded = load_analyzer(name);
    ASSERT_NE(loaded, nullptr);
  }

  Analyzer unspecified_period_proto;
  unspecified_period_proto.mutable_rsi();
  EXPECT_THROW(
      (void)load_analyzer(unspecified_period_proto), std::invalid_argument);

  // Proto with default fields uses period=14, thresholds=30/70, one_minute.
  Analyzer default_proto;
  default_proto.mutable_rsi()->set_candle_period(Analyzer::ONE);
  std::unique_ptr<analyzer> default_loaded = load_analyzer(default_proto);
  ASSERT_NE(default_loaded, nullptr);

  // Proto with custom period, thresholds, and five_minute candle period.
  Analyzer custom_proto;
  Analyzer::Rsi* rsi_config = custom_proto.mutable_rsi();
  rsi_config->set_period(2);
  rsi_config->set_oversold_threshold(20.0);
  rsi_config->set_overbought_threshold(80.0);
  rsi_config->set_candle_period(Analyzer::FIVE);

  std::unique_ptr<analyzer> custom_loaded = load_analyzer(custom_proto);
  ASSERT_NE(custom_loaded, nullptr);

  trading_state state{};
  state.available_funds = 10000.0;
  for (int minute = 1; minute <= 15; ++minute) {
    push_close(state, stock::AAPL, 200.0 - minute, minute);
    decision result = custom_loaded->analyze(stock::AAPL, state);
    if (minute == 15) {
      EXPECT_EQ(result.act, action::BUY);
      EXPECT_DOUBLE_EQ(result.confidence, 1.0);
    } else {
      EXPECT_EQ(result.act, action::NO_ACTION);
    }
  }
}

} // namespace
} // namespace howling
