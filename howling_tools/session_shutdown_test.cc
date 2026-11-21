#include "howling_tools/session_shutdown.h"

#include <chrono>
#include <csignal>
#include <vector>

#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::day;
using ::std::chrono::hours;
using ::std::chrono::milliseconds;
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

TEST(SessionShutdownTest, ComputesDeadlineInEst) {
  // 2026-11-20 09:30 EST is 14:30 UTC; 15:45 EST is 20:45 UTC.
  const system_clock::time_point session_start =
      make_utc(2026, 11, 20, 14, 30, 0);
  const system_clock::time_point expected_deadline =
      make_utc(2026, 11, 20, 20, 45, 0);

  EXPECT_EQ(market_shutdown_deadline(session_start), expected_deadline);
}

TEST(SessionShutdownTest, ComputesDeadlineInEdt) {
  // 2026-07-15 10:00 EDT is 14:00 UTC; 15:45 EDT is 19:45 UTC.
  const system_clock::time_point session_start =
      make_utc(2026, 7, 15, 14, 0, 0);
  const system_clock::time_point expected_deadline =
      make_utc(2026, 7, 15, 19, 45, 0);

  EXPECT_EQ(market_shutdown_deadline(session_start), expected_deadline);
}

TEST(SessionShutdownTest, NormalPreCloseWaitExitsAt1545Et) {
  // Start at 15:44:57 EST (20:44:57 UTC) and expect exit at 15:45:00 EST.
  system_clock::time_point current_time = make_utc(2026, 11, 20, 20, 44, 57);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  std::vector<system_clock::duration> sleeps;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = []() { return false; },
      .sleep =
          [&](system_clock::duration duration) {
            sleeps.push_back(duration);
            current_time += duration;
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_shutdown(deadline, options), shutdown_reason::DEADLINE_REACHED);
  EXPECT_EQ(current_time, deadline);
  ASSERT_EQ(sleeps.size(), 3u);
  EXPECT_EQ(sleeps[0], seconds{1});
  EXPECT_EQ(sleeps[1], seconds{1});
  EXPECT_EQ(sleeps[2], seconds{1});
}

TEST(SessionShutdownTest, ClampsFinalSleepToRemainingDuration) {
  system_clock::time_point current_time =
      make_utc(2026, 11, 20, 20, 44, 59) + milliseconds{750};
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  std::vector<system_clock::duration> sleeps;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = []() { return false; },
      .sleep =
          [&](system_clock::duration duration) {
            sleeps.push_back(duration);
            current_time += duration;
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_shutdown(deadline, options), shutdown_reason::DEADLINE_REACHED);
  ASSERT_EQ(sleeps.size(), 1u);
  EXPECT_EQ(sleeps[0], milliseconds{250});
  EXPECT_EQ(current_time, deadline);
}

TEST(SessionShutdownTest, SuspendResumePastCloseSameDayExitsImmediately) {
  // Session starts at 11:00 EST (16:00 UTC); deadline is 15:45 EST (20:45 UTC).
  system_clock::time_point current_time = make_utc(2026, 11, 20, 16, 0, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  int sleep_count = 0;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = []() { return false; },
      .sleep =
          [&](system_clock::duration) {
            ++sleep_count;
            // Simulate system suspend and resume at 17:30 EST (22:30 UTC).
            current_time = make_utc(2026, 11, 20, 22, 30, 0);
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_shutdown(deadline, options), shutdown_reason::DEADLINE_REACHED);
  EXPECT_EQ(sleep_count, 1);
}

TEST(SessionShutdownTest, SuspendResumePastMidnightEtExitsImmediately) {
  // Session starts at 2026-11-20 09:30 EST (14:30 UTC).
  system_clock::time_point current_time = make_utc(2026, 11, 20, 14, 30, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  int sleep_count = 0;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = []() { return false; },
      .sleep =
          [&](system_clock::duration) {
            ++sleep_count;
            // Simulate waking at 2026-11-21 00:27 EST (2026-11-20 21:27 PST).
            current_time = make_utc(2026, 11, 21, 5, 27, 0);
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_shutdown(deadline, options), shutdown_reason::DEADLINE_REACHED);
  EXPECT_EQ(sleep_count, 1);
}

TEST(SessionShutdownTest, StartingAfterCloseExitsPromptlyWithoutSleeping) {
  // Session starts at 16:00 EST (21:00 UTC), already past 15:45 EST.
  const system_clock::time_point session_start =
      make_utc(2026, 11, 20, 21, 0, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(session_start);
  int sleep_count = 0;

  const wait_options options{
      .now = [&]() { return session_start; },
      .stop_requested = []() { return false; },
      .sleep = [&](system_clock::duration) { ++sleep_count; },
      .poll_interval = seconds{1},
  };

  EXPECT_LT(deadline, session_start);
  EXPECT_EQ(
      wait_for_shutdown(deadline, options), shutdown_reason::DEADLINE_REACHED);
  EXPECT_EQ(sleep_count, 0);
}

TEST(SessionShutdownTest, EarlyExitViaStopSignal) {
  system_clock::time_point current_time = make_utc(2026, 11, 20, 15, 0, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  bool stopped = false;
  int sleep_count = 0;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = [&]() { return stopped; },
      .sleep =
          [&](system_clock::duration duration) {
            ++sleep_count;
            current_time += duration;
            if (sleep_count == 2) stopped = true;
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_shutdown(deadline, options), shutdown_reason::STOP_REQUESTED);
  EXPECT_EQ(sleep_count, 2);
  EXPECT_LT(current_time, deadline);
}

TEST(SessionShutdownTest, CatchUpReturnsWhenStateTimeCatchesUp) {
  system_clock::time_point current_time = make_utc(2026, 11, 20, 15, 0, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  system_clock::time_point state_time{};
  int sleep_count = 0;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = []() { return false; },
      .sleep =
          [&](system_clock::duration duration) {
            ++sleep_count;
            current_time += duration;
            if (sleep_count == 2) state_time = current_time - seconds{30};
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_catch_up(
          [&]() { return state_time; }, deadline, minutes{2}, options),
      shutdown_reason::CAUGHT_UP);
  EXPECT_EQ(sleep_count, 2);
}

TEST(SessionShutdownTest, CatchUpExitsAtDeadlineWhenStreamIsStalled) {
  system_clock::time_point current_time = make_utc(2026, 11, 20, 20, 44, 58);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  const system_clock::time_point stalled_time{};
  int sleep_count = 0;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = []() { return false; },
      .sleep =
          [&](system_clock::duration duration) {
            ++sleep_count;
            current_time += duration;
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_catch_up(
          [&]() { return stalled_time; }, deadline, minutes{2}, options),
      shutdown_reason::DEADLINE_REACHED);
  EXPECT_EQ(sleep_count, 2);
}

TEST(SessionShutdownTest, CatchUpExitsImmediatelyWhenStartedAfterClose) {
  const system_clock::time_point session_start =
      make_utc(2026, 11, 20, 21, 0, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(session_start);
  int sleep_count = 0;

  const wait_options options{
      .now = [&]() { return session_start; },
      .stop_requested = []() { return false; },
      .sleep = [&](system_clock::duration) { ++sleep_count; },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_catch_up(
          []() { return system_clock::time_point{}; },
          deadline,
          minutes{2},
          options),
      shutdown_reason::DEADLINE_REACHED);
  EXPECT_EQ(sleep_count, 0);
}

TEST(SessionShutdownTest, CatchUpExitsOnStopSignalWhenStreamIsStalled) {
  system_clock::time_point current_time = make_utc(2026, 11, 20, 15, 0, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);
  bool stopped = false;

  const wait_options options{
      .now = [&]() { return current_time; },
      .stop_requested = [&]() { return stopped; },
      .sleep =
          [&](system_clock::duration duration) {
            current_time += duration;
            stopped = true;
          },
      .poll_interval = seconds{1},
  };

  EXPECT_EQ(
      wait_for_catch_up(
          []() { return system_clock::time_point{}; },
          deadline,
          minutes{2},
          options),
      shutdown_reason::STOP_REQUESTED);
}

TEST(SessionShutdownTest, ScopedSignalHandlerHandlesSigtermAndSigint) {
  system_clock::time_point current_time = make_utc(2026, 11, 20, 15, 0, 0);
  const system_clock::time_point deadline =
      market_shutdown_deadline(current_time);

  {
    scoped_signal_handler handler;
    EXPECT_FALSE(shutdown_signal_received());
    EXPECT_EQ(received_shutdown_signal(), 0);

    const wait_options options{
        .now = [&]() { return current_time; },
        .stop_requested = []() { return shutdown_signal_received(); },
        .sleep =
            [&](system_clock::duration duration) {
              current_time += duration;
              ASSERT_EQ(std::raise(SIGTERM), 0);
            },
        .poll_interval = seconds{1},
    };

    EXPECT_EQ(
        wait_for_shutdown(deadline, options), shutdown_reason::STOP_REQUESTED);
    EXPECT_TRUE(shutdown_signal_received());
    EXPECT_EQ(received_shutdown_signal(), SIGTERM);

    reset_shutdown_signal();
    EXPECT_FALSE(shutdown_signal_received());

    ASSERT_EQ(std::raise(SIGINT), 0);
    EXPECT_TRUE(shutdown_signal_received());
    EXPECT_EQ(received_shutdown_signal(), SIGINT);
  }

  reset_shutdown_signal();
  EXPECT_FALSE(shutdown_signal_received());
}

} // namespace
} // namespace howling
