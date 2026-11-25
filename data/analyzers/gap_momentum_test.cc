#include "data/analyzers/gap_momentum.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
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

// Constructs a UTC time_point corresponding to the given Eastern Standard Time
// (UTC-5) clock time on a winter date (e.g., November 2026).
system_clock::time_point make_est(
    int year_value,
    unsigned month_value,
    unsigned day_value,
    int hour_et,
    int minute_et) {
  return sys_days{year{year_value} / month{month_value} / day{day_value}} +
      hours{hour_et + 5} + minutes{minute_et};
}

Candle make_candle(
    system_clock::time_point opened_at,
    double open,
    double high,
    double low,
    double close,
    int64_t volume) {
  Candle candle;
  *candle.mutable_opened_at() = to_proto(opened_at);
  candle.set_open(open);
  candle.set_high(high);
  candle.set_low(low);
  candle.set_close(close);
  candle.set_volume(volume);
  return candle;
}

void push_candle(
    trading_state& state, stock::Symbol symbol, const Candle& candle) {
  add_next_minute(state.market[symbol], candle);
  state.time_now = to_std_chrono(candle.opened_at());
}

// Populates a baseline trading day with 5 opening-window candles (09:30-09:34
// ET) and a closing candle at 15:59 ET.
void add_baseline_day(
    trading_state& state,
    stock::Symbol symbol,
    gap_momentum_analyzer& analyzer,
    unsigned day_of_month,
    double open_price,
    double close_price,
    int64_t per_minute_open_volume) {
  for (int minute = 0; minute < 5; ++minute) {
    push_candle(
        state,
        symbol,
        make_candle(
            make_est(2026, 11, day_of_month, 9, 30 + minute),
            open_price,
            open_price + 0.5,
            open_price - 0.5,
            open_price,
            per_minute_open_volume));
    (void)analyzer.analyze(symbol, state);
  }
  push_candle(
      state,
      symbol,
      make_candle(
          make_est(2026, 11, day_of_month, 15, 59),
          close_price,
          close_price + 0.2,
          close_price - 0.2,
          close_price,
          500));
  (void)analyzer.analyze(symbol, state);
}

TEST(GapMomentumAnalyzerTest, RejectsInvalidOptions) {
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.momentum_gap_threshold = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.momentum_gap_threshold = -0.01}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{
              .momentum_gap_threshold =
                  std::numeric_limits<double>::quiet_NaN()}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{
              .momentum_gap_threshold =
                  std::numeric_limits<double>::infinity()}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.fill_gap_threshold = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.high_relative_volume = 0.0}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.low_relative_volume = -0.5}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.opening_window_minutes = 0}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.active_window_minutes = 0}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{
              .opening_window_minutes = 10, .active_window_minutes = 5}),
      std::invalid_argument);
  EXPECT_THROW(
      gap_momentum_analyzer(
          gap_momentum_analyzer::options{.volume_baseline_days = 0}),
      std::invalid_argument);
}

TEST(GapMomentumAnalyzerTest, ReturnsNoActionOnDayOneAndExtendedHours) {
  gap_momentum_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  // Missing symbol returns NO_ACTION.
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Pre-market candle on day 1 returns NO_ACTION.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 16, 9, 15), 100.0, 101.0, 99.0, 100.5, 500));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Regular-hours candles on day 1 have no prior session or volume baseline.
  for (int minute = 0; minute < 10; ++minute) {
    push_candle(
        state,
        stock::AAPL,
        make_candle(
            make_est(2026, 11, 16, 9, 30 + minute),
            100.0,
            102.0,
            99.5,
            101.5 + minute * 0.2,
            1000));
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  }

  // After-hours candle returns NO_ACTION.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 16, 16, 5), 105.0, 106.0, 104.0, 105.5, 500));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
}

TEST(GapMomentumAnalyzerTest, GapAndGoBuysAfterBreakoutAndWaitsBefore) {
  gap_momentum_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  // Day 1: opening-window total volume = 5 * 1000 = 5000, close = 100.0.
  add_baseline_day(state, stock::AAPL, analyzer, 16, 100.0, 100.0, 1000);

  // Day 2 pre-market candle must be ignored for today_open and opening volume.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 9, 20), 95.0, 96.0, 94.0, 95.0, 99999));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Day 2 opening window (09:30-09:34 ET): +2.5% gap (open 102.5 vs prev close
  // 100.0), 2.0x relative volume (5 * 2000 = 10000 vs 5000), opening high
  // 103.5.
  for (int minute = 0; minute < 5; ++minute) {
    push_candle(
        state,
        stock::AAPL,
        make_candle(
            make_est(2026, 11, 17, 9, 30 + minute),
            102.5,
            103.5,
            102.0,
            103.0,
            2000));
    // During the initial opening window, analyzer always returns NO_ACTION.
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  }

  // At 09:35 ET, close is still below opening_range_high (103.5) -> NO_ACTION.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 9, 35), 103.0, 103.4, 102.8, 103.4, 800));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // At 09:36 ET, close breaks above opening_range_high (103.5) -> BUY.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 9, 36), 103.4, 104.2, 103.2, 104.0, 900));
  decision buy_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(buy_decision.act, action::BUY);
  EXPECT_GT(buy_decision.confidence, 0.0);
  EXPECT_LE(buy_decision.confidence, 1.0);

  // If insufficient funds (can_buy == false), returns NO_ACTION.
  state.available_funds = 10.0;
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
}

TEST(GapMomentumAnalyzerTest, GapFillSellsWhenHoldingAndHoldsWhenFlat) {
  gap_momentum_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  // Day 1: opening-window total volume = 5 * 1000 = 5000, close = 100.0.
  add_baseline_day(state, stock::AAPL, analyzer, 16, 100.0, 100.0, 1000);

  // Day 2: +3.5% gap (open 103.5 vs 100.0), 0.5x relative volume (5 * 500 =
  // 2500 vs 5000).
  for (int minute = 0; minute < 5; ++minute) {
    push_candle(
        state,
        stock::AAPL,
        make_candle(
            make_est(2026, 11, 17, 9, 30 + minute),
            103.5,
            104.0,
            103.0,
            103.2,
            500));
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  }

  // At 09:35 ET with no position held -> HOLD.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 9, 35), 103.2, 103.4, 102.9, 103.0, 400));
  decision hold_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(hold_decision.act, action::HOLD);
  EXPECT_GT(hold_decision.confidence, 0.0);
  EXPECT_LE(hold_decision.confidence, 1.0);

  // With shares held -> SELL.
  state.positions[stock::AAPL].push_back(
      {.symbol = stock::AAPL, .price = 100.0, .quantity = 10});
  decision sell_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(sell_decision.act, action::SELL);
  EXPECT_GT(sell_decision.confidence, 0.0);
  EXPECT_LE(sell_decision.confidence, 1.0);
}

TEST(GapMomentumAnalyzerTest, ReturnsNoActionOutsideActiveWindow) {
  gap_momentum_analyzer analyzer(
      gap_momentum_analyzer::options{.active_window_minutes = 90});
  trading_state state{};
  state.available_funds = 10000.0;

  add_baseline_day(state, stock::AAPL, analyzer, 16, 100.0, 100.0, 1000);

  // Day 2: Gap and Go setup (+2.5% gap, 2.0x relative volume).
  for (int minute = 0; minute < 5; ++minute) {
    push_candle(
        state,
        stock::AAPL,
        make_candle(
            make_est(2026, 11, 17, 9, 30 + minute),
            102.5,
            103.5,
            102.0,
            103.0,
            2000));
    (void)analyzer.analyze(stock::AAPL, state);
  }

  // At 11:00 ET (90 minutes since open), still inside active window -> BUY.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 11, 0), 103.6, 104.5, 103.5, 104.2, 500));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::BUY);

  // At 11:01 ET (91 minutes since open), outside active window -> NO_ACTION.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 11, 1), 104.2, 105.0, 104.0, 104.8, 500));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
}

TEST(GapMomentumAnalyzerTest, IsolatesStateAcrossMultipleSymbols) {
  gap_momentum_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;
  state.positions[stock::NVDA].push_back(
      {.symbol = stock::NVDA, .price = 200.0, .quantity = 5});

  // Day 1 baselines for both AAPL and NVDA.
  add_baseline_day(state, stock::AAPL, analyzer, 16, 100.0, 100.0, 1000);
  add_baseline_day(state, stock::NVDA, analyzer, 16, 200.0, 200.0, 2000);

  // Day 2 interleaved: AAPL sets up Gap and Go (+2.5%, 2.0x vol), NVDA sets up
  // Gap Fill (+4.0%, 0.5x vol).
  for (int minute = 0; minute < 5; ++minute) {
    push_candle(
        state,
        stock::AAPL,
        make_candle(
            make_est(2026, 11, 17, 9, 30 + minute),
            102.5,
            103.5,
            102.0,
            103.0,
            2000));
    (void)analyzer.analyze(stock::AAPL, state);

    push_candle(
        state,
        stock::NVDA,
        make_candle(
            make_est(2026, 11, 17, 9, 30 + minute),
            208.0,
            209.0,
            207.0,
            207.5,
            1000));
    (void)analyzer.analyze(stock::NVDA, state);
  }

  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 9, 35), 103.5, 104.5, 103.4, 104.2, 800));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::BUY);

  push_candle(
      state,
      stock::NVDA,
      make_candle(
          make_est(2026, 11, 17, 9, 35), 207.5, 207.8, 206.8, 207.0, 500));
  EXPECT_EQ(analyzer.analyze(stock::NVDA, state).act, action::SELL);
}

TEST(GapMomentumAnalyzerTest, GuardsAgainstZeroDenominators) {
  // Zero previous close -> NO_ACTION.
  {
    gap_momentum_analyzer analyzer;
    trading_state state{};
    state.available_funds = 10000.0;
    add_baseline_day(state, stock::AAPL, analyzer, 16, 100.0, 0.0, 1000);
    for (int minute = 0; minute < 6; ++minute) {
      push_candle(
          state,
          stock::AAPL,
          make_candle(
              make_est(2026, 11, 17, 9, 30 + minute),
              103.0,
              104.0,
              102.0,
              105.0,
              2000));
    }
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  }

  // Zero prior opening volume baseline -> NO_ACTION.
  {
    gap_momentum_analyzer analyzer;
    trading_state state{};
    state.available_funds = 10000.0;
    add_baseline_day(state, stock::AAPL, analyzer, 16, 100.0, 100.0, 0);
    for (int minute = 0; minute < 6; ++minute) {
      push_candle(
          state,
          stock::AAPL,
          make_candle(
              make_est(2026, 11, 17, 9, 30 + minute),
              103.0,
              104.0,
              102.0,
              105.0,
              2000));
    }
    EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  }
}

TEST(GapMomentumAnalyzerTest, LoadAnalyzerWiresNameAndProtoConfig) {
  std::unique_ptr<analyzer> by_name = load_analyzer("gap_momentum");
  ASSERT_NE(by_name, nullptr);

  Analyzer proto_config;
  Analyzer::GapMomentum* gap_config = proto_config.mutable_gap_momentum();
  gap_config->set_momentum_gap_threshold(0.01);
  gap_config->set_high_relative_volume(1.2);
  gap_config->set_opening_window_minutes(3);
  gap_config->set_active_window_minutes(30);
  gap_config->set_volume_baseline_days(5);

  std::unique_ptr<analyzer> by_proto = load_analyzer(proto_config);
  ASSERT_NE(by_proto, nullptr);

  trading_state state{};
  state.available_funds = 10000.0;
  // Day 1: 3-minute opening window (3 * 1000 = 3000), close = 100.0.
  for (int minute = 0; minute < 3; ++minute) {
    push_candle(
        state,
        stock::AAPL,
        make_candle(
            make_est(2026, 11, 16, 9, 30 + minute),
            100.0,
            100.5,
            99.5,
            100.0,
            1000));
    (void)by_proto->analyze(stock::AAPL, state);
  }
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 16, 15, 59), 100.0, 100.2, 99.8, 100.0, 500));
  (void)by_proto->analyze(stock::AAPL, state);

  // Day 2: +1.5% gap (> 1% custom threshold), 1.5x vol (> 1.2x custom
  // threshold), 3-minute opening window.
  for (int minute = 0; minute < 3; ++minute) {
    push_candle(
        state,
        stock::AAPL,
        make_candle(
            make_est(2026, 11, 17, 9, 30 + minute),
            101.5,
            102.0,
            101.0,
            101.8,
            1500));
    EXPECT_EQ(by_proto->analyze(stock::AAPL, state).act, action::NO_ACTION);
  }
  // At 09:33 ET (3 minutes after open), breakout above 102.0 -> BUY.
  push_candle(
      state,
      stock::AAPL,
      make_candle(
          make_est(2026, 11, 17, 9, 33), 101.9, 102.6, 101.8, 102.5, 600));
  EXPECT_EQ(by_proto->analyze(stock::AAPL, state).act, action::BUY);
}

} // namespace
} // namespace howling
