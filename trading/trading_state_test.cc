#include "trading/trading_state.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>
#include <utility>

#include "time/thread_safe.h"
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

TEST(TradingStateTest, MarketTimeAccessorsRespectEstAndOpenBounds) {
  trading_state state{};

  // 2026-11-20 is in EST (UTC-5): 14:29:59 UTC is 09:29:59 ET.
  state.time_now = make_utc(2026, 11, 20, 14, 29, 59);
  EXPECT_EQ(state.market_hour(), 9);
  EXPECT_EQ(state.market_minute(), 29);
  EXPECT_EQ(state.market_second(), 59);
  EXPECT_FALSE(state.market_is_open());

  // 14:30:00 UTC is 09:30:00 ET (market open).
  state.time_now = make_utc(2026, 11, 20, 14, 30, 0);
  EXPECT_EQ(state.market_hour(), 9);
  EXPECT_EQ(state.market_minute(), 30);
  EXPECT_EQ(state.market_second(), 0);
  EXPECT_TRUE(state.market_is_open());

  // 20:59:59 UTC is 15:59:59 ET (last second before close).
  state.time_now = make_utc(2026, 11, 20, 20, 59, 59);
  EXPECT_EQ(state.market_hour(), 15);
  EXPECT_EQ(state.market_minute(), 59);
  EXPECT_EQ(state.market_second(), 59);
  EXPECT_TRUE(state.market_is_open());

  // 21:00:00 UTC is 16:00:00 ET (market closed).
  state.time_now = make_utc(2026, 11, 20, 21, 0, 0);
  EXPECT_EQ(state.market_hour(), 16);
  EXPECT_EQ(state.market_minute(), 0);
  EXPECT_EQ(state.market_second(), 0);
  EXPECT_FALSE(state.market_is_open());
}

TEST(TradingStateTest, MarketTimeAccessorsRespectEdt) {
  trading_state state{};

  // 2026-07-15 is in EDT (UTC-4): 19:45:12 UTC is 15:45:12 ET.
  state.time_now = make_utc(2026, 7, 15, 19, 45, 12);
  EXPECT_EQ(state.market_hour(), 15);
  EXPECT_EQ(state.market_minute(), 45);
  EXPECT_EQ(state.market_second(), 12);
  EXPECT_TRUE(state.market_is_open());
}

TEST(TradingStateTest, AggregateInitializationAndMovePreserveTimeNow) {
  static_assert(std::is_aggregate_v<trading_state>);
  static_assert(
      std::is_same_v<decltype(trading_state::time_now), atomic_time_point>);

  const system_clock::time_point expected_time =
      make_utc(2026, 11, 20, 15, 0, 0);
  trading_state original{
      .account_id = "account-1",
      .initial_funds = 20'000.0,
      .available_funds = 20'000.0,
      .time_now = expected_time};

  // Implicit conversion to system_clock::time_point works seamlessly.
  const system_clock::time_point loaded_time = original.time_now;
  EXPECT_EQ(loaded_time, expected_time);

  trading_state moved = std::move(original);
  EXPECT_EQ(moved.account_id, "account-1");
  EXPECT_EQ(moved.time_now.load(), expected_time);
}

TEST(TradingStateTest, ConcurrentTimeNowReadAndWriteIsThreadSafe) {
  const system_clock::time_point base_time = make_utc(2026, 11, 20, 14, 30, 0);
  constexpr int ITERATIONS = 10'000;
  trading_state state{.time_now = base_time};

  std::atomic<bool> writer_done{false};
  std::jthread writer([&]() {
    for (int i = 1; i <= ITERATIONS; ++i) {
      state.time_now.store(base_time + seconds{i});
    }
    writer_done.store(true, std::memory_order_release);
  });

  system_clock::time_point previous_observed = base_time;
  const system_clock::time_point max_time = base_time + seconds{ITERATIONS};
  while (!writer_done.load(std::memory_order_acquire)) {
    const system_clock::time_point observed = state.time_now.load();
    EXPECT_GE(observed, previous_observed);
    EXPECT_LE(observed, max_time);
    previous_observed = observed;

    // Exercise market-time accessors concurrently with updates.
    const int hour = state.market_hour();
    EXPECT_GE(hour, 9);
    EXPECT_LE(hour, 13);
  }

  writer.join();
  EXPECT_EQ(state.time_now.load(), max_time);
}

} // namespace
} // namespace howling
