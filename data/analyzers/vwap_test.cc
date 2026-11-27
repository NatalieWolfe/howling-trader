#include "data/analyzers/vwap.h"

#include <chrono>
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
    double high,
    double low,
    double close,
    int64_t volume,
    system_clock::time_point opened_at) {
  Candle candle;
  candle.set_open((high + low) / 2.0);
  candle.set_high(high);
  candle.set_low(low);
  candle.set_close(close);
  candle.set_volume(volume);
  *candle.mutable_opened_at() = to_proto(opened_at);
  return candle;
}

void append_candle(
    trading_state& state,
    stock::Symbol symbol,
    double high,
    double low,
    double close,
    int64_t volume,
    system_clock::time_point opened_at) {
  add_next_minute(
      state.market[symbol], make_candle(high, low, close, volume, opened_at));
  state.time_now = opened_at;
}

TEST(VwapAnalyzerTest, RejectsInvalidOptions) {
  EXPECT_THROW(
      vwap_analyzer(vwap_analyzer::options{.min_candles = 0}),
      std::invalid_argument);
  EXPECT_THROW(
      vwap_analyzer(vwap_analyzer::options{.min_candles = -3}),
      std::invalid_argument);
  EXPECT_THROW(
      vwap_analyzer(vwap_analyzer::options{.deviation_threshold = -0.001}),
      std::invalid_argument);
  EXPECT_THROW(
      vwap_analyzer(
          vwap_analyzer::options{
              .deviation_threshold = std::numeric_limits<double>::quiet_NaN()}),
      std::invalid_argument);
  EXPECT_THROW(
      vwap_analyzer(
          vwap_analyzer::options{
              .deviation_threshold = std::numeric_limits<double>::infinity()}),
      std::invalid_argument);

  EXPECT_NO_THROW(
      vwap_analyzer(vwap_analyzer::options{.deviation_threshold = 0.0}));
}

TEST(VwapAnalyzerTest, ReturnsNoActionWhenMarketMissingOrEmpty) {
  vwap_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_FALSE(analyzer.current_vwap(stock::AAPL).has_value());

  state.market[stock::AAPL];
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_FALSE(analyzer.current_vwap(stock::AAPL).has_value());
}

TEST(VwapAnalyzerTest, CalculatesHandVerifiedVwap) {
  vwap_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  // 14:30 UTC in January (EST) is 09:30 ET.
  const system_clock::time_point base_time = make_utc(2026, 1, 15, 14, 30);

  // Bar 1: typical = (103 + 97 + 100) / 3 = 100.0, volume = 100 -> VWAP = 100.
  append_candle(state, stock::AAPL, 103.0, 97.0, 100.0, 100, base_time);
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 100.0, 1e-9);

  // Bar 2: typical = (112 + 106 + 109) / 3 = 109.0, volume = 300.
  // VWAP = (100 * 100 + 109 * 300) / 400 = 42700 / 400 = 106.75.
  append_candle(
      state, stock::AAPL, 112.0, 106.0, 109.0, 300, base_time + minutes{1});
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 106.75, 1e-9);

  // Append two bars before calling analyze() to verify incremental catch-up.
  // Bar 3: typical = (106 + 100 + 103) / 3 = 103.0, volume = 200.
  // Bar 4: typical = (120 + 114 + 117) / 3 = 117.0, volume = 400.
  // VWAP = (42700 + 103 * 200 + 117 * 400) / 1000 = 110100 / 1000 = 110.1.
  append_candle(
      state, stock::AAPL, 106.0, 100.0, 103.0, 200, base_time + minutes{2});
  append_candle(
      state, stock::AAPL, 120.0, 114.0, 117.0, 400, base_time + minutes{3});
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 110.1, 1e-9);

  // Calling analyze() again with no new candles does not double-count.
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 110.1, 1e-9);
}

TEST(VwapAnalyzerTest, ResetsDailyAcrossSessionBoundaryEstAndEdt) {
  vwap_analyzer analyzer(vwap_analyzer::options{.min_candles = 2});
  trading_state state{};
  state.available_funds = 10000.0;

  // EST Day 1: 2026-01-15 14:30 UTC (09:30 EST).
  append_candle(
      state,
      stock::AAPL,
      101.0,
      99.0,
      100.0,
      10000,
      make_utc(2026, 1, 15, 14, 30));
  append_candle(
      state,
      stock::AAPL,
      101.0,
      99.0,
      100.0,
      10000,
      make_utc(2026, 1, 15, 14, 31));
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 100.0, 1e-9);

  // EST Day 2: 2026-01-16 14:30 UTC (09:30 EST) with small volume at $200.
  append_candle(
      state,
      stock::AAPL,
      202.0,
      198.0,
      200.0,
      50,
      make_utc(2026, 1, 16, 14, 30));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 200.0, 1e-9);

  // EDT Day 1: 2026-07-15 13:30 UTC (09:30 EDT).
  append_candle(
      state,
      stock::AAPL,
      151.0,
      149.0,
      150.0,
      5000,
      make_utc(2026, 7, 15, 13, 30));
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 150.0, 1e-9);

  // EDT Day 2: 2026-07-16 13:30 UTC (09:30 EDT).
  append_candle(
      state,
      stock::AAPL,
      303.0,
      297.0,
      300.0,
      10,
      make_utc(2026, 7, 16, 13, 30));
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 300.0, 1e-9);
}

TEST(VwapAnalyzerTest, FiltersExtendedHoursWhenDisabled) {
  vwap_analyzer analyzer(
      vwap_analyzer::options{
          .min_candles = 1, .include_extended_hours = false});
  trading_state state{};
  state.available_funds = 10000.0;

  // 14:29 UTC in EST is 09:29 ET (pre-market).
  append_candle(
      state,
      stock::AAPL,
      50.0,
      50.0,
      50.0,
      1000,
      make_utc(2026, 1, 15, 14, 29));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_FALSE(analyzer.current_vwap(stock::AAPL).has_value());

  // 14:30 UTC in EST is 09:30 ET (regular market open).
  append_candle(
      state,
      stock::AAPL,
      100.0,
      100.0,
      100.0,
      100,
      make_utc(2026, 1, 15, 14, 30));
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 100.0, 1e-9);

  // 20:59 UTC in EST is 15:59 ET (last regular minute).
  append_candle(
      state,
      stock::AAPL,
      110.0,
      110.0,
      110.0,
      100,
      make_utc(2026, 1, 15, 20, 59));
  analyzer.analyze(stock::AAPL, state);
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 105.0, 1e-9);

  // 21:00 UTC in EST is 16:00 ET (after-hours, excluded).
  append_candle(
      state,
      stock::AAPL,
      500.0,
      500.0,
      500.0,
      9999,
      make_utc(2026, 1, 15, 21, 0));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 105.0, 1e-9);

  // Also verify EDT boundaries: 13:29 UTC (09:29 EDT) is pre-market and resets
  // the session day without accumulating.
  append_candle(
      state,
      stock::AAPL,
      80.0,
      80.0,
      80.0,
      1000,
      make_utc(2026, 7, 15, 13, 29));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_FALSE(analyzer.current_vwap(stock::AAPL).has_value());

  // 13:30 UTC (09:30 EDT) is regular market open.
  append_candle(
      state,
      stock::AAPL,
      200.0,
      200.0,
      200.0,
      50,
      make_utc(2026, 7, 15, 13, 30));
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 200.0, 1e-9);

  // 20:00 UTC (16:00 EDT) is after-hours, excluded.
  append_candle(
      state,
      stock::AAPL,
      400.0,
      400.0,
      400.0,
      500,
      make_utc(2026, 7, 15, 20, 0));
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 200.0, 1e-9);
}

TEST(VwapAnalyzerTest, IncludesExtendedHoursWhenEnabled) {
  vwap_analyzer analyzer(
      vwap_analyzer::options{.min_candles = 1, .include_extended_hours = true});
  trading_state state{};
  state.available_funds = 10000.0;

  // 13:00 UTC in EST is 08:00 ET (pre-market).
  append_candle(
      state,
      stock::AAPL,
      100.0,
      100.0,
      100.0,
      100,
      make_utc(2026, 1, 15, 13, 0));
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 100.0, 1e-9);

  // 22:00 UTC in EST is 17:00 ET (after-hours).
  append_candle(
      state,
      stock::AAPL,
      120.0,
      120.0,
      120.0,
      100,
      make_utc(2026, 1, 15, 22, 0));
  analyzer.analyze(stock::AAPL, state);
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 110.0, 1e-9);
}

TEST(VwapAnalyzerTest, GuardsZeroVolumeCandles) {
  vwap_analyzer analyzer(vwap_analyzer::options{.min_candles = 1});
  trading_state state{};
  state.available_funds = 10000.0;

  const system_clock::time_point base_time = make_utc(2026, 1, 15, 14, 30);
  append_candle(state, stock::AAPL, 100.0, 100.0, 100.0, 0, base_time);
  const decision opening_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(opening_decision.act, action::NO_ACTION);
  EXPECT_FALSE(std::isnan(opening_decision.confidence));
  EXPECT_FALSE(analyzer.current_vwap(stock::AAPL).has_value());

  // Positive volume establishes VWAP.
  append_candle(
      state, stock::AAPL, 105.0, 105.0, 105.0, 200, base_time + minutes{1});
  analyzer.analyze(stock::AAPL, state);
  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 105.0, 1e-9);

  // Subsequent zero-volume bar preserves VWAP without NaN.
  append_candle(
      state, stock::AAPL, 120.0, 120.0, 120.0, 0, base_time + minutes{2});
  const decision zero_volume_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_FALSE(std::isnan(zero_volume_decision.confidence));
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 105.0, 1e-9);
}

TEST(VwapAnalyzerTest, TrendFilterEmitsCrossoverSignalsRespectingGuards) {
  vwap_analyzer analyzer(
      vwap_analyzer::options{
          .strategy_mode = vwap_analyzer::mode::TREND_FILTER,
          .deviation_threshold = 0.01,
          .min_candles = 3});
  trading_state state{};
  state.available_funds = 10000.0;

  const system_clock::time_point base_time = make_utc(2026, 1, 15, 14, 30);

  // Bar 1 (warmup): close = 100, VWAP = 100.
  append_candle(state, stock::AAPL, 100.0, 100.0, 100.0, 1000, base_time);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Bar 2 (warmup): crosses above VWAP, still in warmup -> NO_ACTION.
  append_candle(
      state, stock::AAPL, 105.0, 105.0, 105.0, 100, base_time + minutes{1});
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Bar 3: pulls back below VWAP, no position held -> NO_ACTION.
  append_candle(
      state, stock::AAPL, 98.0, 98.0, 98.0, 100, base_time + minutes{2});
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Bar 4: crosses above VWAP with available funds -> BUY.
  append_candle(
      state, stock::AAPL, 104.0, 104.0, 104.0, 100, base_time + minutes{3});
  const decision buy_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(buy_decision.act, action::BUY);
  EXPECT_GE(buy_decision.confidence, 0.1);
  EXPECT_LE(buy_decision.confidence, 1.0);

  // Bar 5: remains above VWAP (no crossover) -> NO_ACTION.
  append_candle(
      state, stock::AAPL, 105.0, 105.0, 105.0, 100, base_time + minutes{4});
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Bar 6: crosses below VWAP with held position -> SELL.
  state.positions[stock::AAPL].push_back(
      trading_state::position{
          .symbol = stock::AAPL, .price = 104.0, .quantity = 10});
  append_candle(
      state, stock::AAPL, 97.0, 97.0, 97.0, 100, base_time + minutes{5});
  const decision sell_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(sell_decision.act, action::SELL);
  EXPECT_GE(sell_decision.confidence, 0.1);
  EXPECT_LE(sell_decision.confidence, 1.0);

  // Bar 7: crosses above VWAP again, but insufficient funds -> NO_ACTION.
  state.available_funds = 10.0;
  append_candle(
      state, stock::AAPL, 106.0, 106.0, 106.0, 100, base_time + minutes{6});
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);
}

TEST(VwapAnalyzerTest, MeanReversionEmitsDeviationSignalsRespectingGuards) {
  vwap_analyzer analyzer(
      vwap_analyzer::options{
          .strategy_mode = vwap_analyzer::mode::MEAN_REVERSION,
          .deviation_threshold = 0.02,
          .min_candles = 2});
  trading_state state{};
  state.available_funds = 10000.0;

  const system_clock::time_point base_time = make_utc(2026, 1, 15, 14, 30);

  // Bar 1 (warmup): establish VWAP near 100.
  append_candle(state, stock::AAPL, 100.0, 100.0, 100.0, 10000, base_time);
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Bar 2: within +/-2% neutral band -> NO_ACTION.
  append_candle(
      state, stock::AAPL, 99.0, 99.0, 99.0, 10, base_time + minutes{1});
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Bar 3: close = 96.0 (~4% below VWAP) with funds -> BUY.
  append_candle(
      state, stock::AAPL, 96.0, 96.0, 96.0, 10, base_time + minutes{2});
  const decision buy_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(buy_decision.act, action::BUY);
  EXPECT_GE(buy_decision.confidence, 0.1);
  EXPECT_LE(buy_decision.confidence, 1.0);

  // Bar 4: close = 104.0 (~4% above VWAP), but no position -> NO_ACTION.
  append_candle(
      state, stock::AAPL, 104.0, 104.0, 104.0, 10, base_time + minutes{3});
  EXPECT_EQ(analyzer.analyze(stock::AAPL, state).act, action::NO_ACTION);

  // Bar 5: close = 104.0 with held position -> SELL.
  state.positions[stock::AAPL].push_back(
      trading_state::position{
          .symbol = stock::AAPL, .price = 96.0, .quantity = 5});
  append_candle(
      state, stock::AAPL, 104.0, 104.0, 104.0, 10, base_time + minutes{4});
  const decision sell_decision = analyzer.analyze(stock::AAPL, state);
  EXPECT_EQ(sell_decision.act, action::SELL);
  EXPECT_GE(sell_decision.confidence, 0.1);
  EXPECT_LE(sell_decision.confidence, 1.0);
}

TEST(VwapAnalyzerTest, IsolatesStateAcrossSymbols) {
  vwap_analyzer analyzer;
  trading_state state{};
  state.available_funds = 10000.0;

  const system_clock::time_point base_time = make_utc(2026, 1, 15, 14, 30);
  append_candle(state, stock::AAPL, 100.0, 100.0, 100.0, 100, base_time);
  append_candle(state, stock::NVDA, 500.0, 500.0, 500.0, 200, base_time);

  analyzer.analyze(stock::AAPL, state);
  analyzer.analyze(stock::NVDA, state);

  ASSERT_TRUE(analyzer.current_vwap(stock::AAPL).has_value());
  ASSERT_TRUE(analyzer.current_vwap(stock::NVDA).has_value());
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 100.0, 1e-9);
  EXPECT_NEAR(*analyzer.current_vwap(stock::NVDA), 500.0, 1e-9);

  // Update only NVDA and verify AAPL is unaffected.
  append_candle(
      state, stock::NVDA, 700.0, 700.0, 700.0, 200, base_time + minutes{1});
  analyzer.analyze(stock::NVDA, state);
  EXPECT_NEAR(*analyzer.current_vwap(stock::AAPL), 100.0, 1e-9);
  EXPECT_NEAR(*analyzer.current_vwap(stock::NVDA), 600.0, 1e-9);
}

TEST(VwapAnalyzerTest, LoadsFromNameAndProto) {
  std::unique_ptr<analyzer> by_name = load_analyzer("vwap");
  ASSERT_NE(by_name, nullptr);

  Analyzer default_proto;
  default_proto.mutable_vwap();
  std::unique_ptr<analyzer> by_default_proto = load_analyzer(default_proto);
  ASSERT_NE(by_default_proto, nullptr);

  Analyzer mean_reversion_proto;
  mean_reversion_proto.mutable_vwap()->set_mode(Analyzer::Vwap::MEAN_REVERSION);
  mean_reversion_proto.mutable_vwap()->set_deviation_threshold(0.01);
  mean_reversion_proto.mutable_vwap()->set_min_candles(2);
  mean_reversion_proto.mutable_vwap()->set_include_extended_hours(true);

  std::unique_ptr<analyzer> configured = load_analyzer(mean_reversion_proto);
  ASSERT_NE(configured, nullptr);

  // Verify proto options took effect (extended hours + mean reversion).
  trading_state state{};
  state.available_funds = 10000.0;
  // 13:00 UTC in EST is 08:00 ET (pre-market).
  append_candle(
      state,
      stock::AAPL,
      100.0,
      100.0,
      100.0,
      1000,
      make_utc(2026, 1, 15, 13, 0));
  EXPECT_EQ(configured->analyze(stock::AAPL, state).act, action::NO_ACTION);
  append_candle(
      state, stock::AAPL, 95.0, 95.0, 95.0, 10, make_utc(2026, 1, 15, 13, 1));
  EXPECT_EQ(configured->analyze(stock::AAPL, state).act, action::BUY);

  // Invalid proto options throw.
  Analyzer invalid_proto;
  invalid_proto.mutable_vwap()->set_min_candles(-1);
  EXPECT_THROW((void)load_analyzer(invalid_proto), std::invalid_argument);
}

} // namespace
} // namespace howling
