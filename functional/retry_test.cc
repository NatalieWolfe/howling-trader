#include "functional/retry.h"

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

#include "signaling/cancellation.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::system_clock;
using ::testing::HasSubstr;
using ::testing::ThrowsMessage;
using namespace std::chrono_literals;

TEST(BackoffTest, SucceedsOnFirstAttempt) {
  int attempt_count = 0;
  backoff([&]() { ++attempt_count; });
  EXPECT_EQ(attempt_count, 1);
}

TEST(BackoffTest, RetriesUntilSuccess) {
  int attempt_count = 0;
  int result = backoff(
      backoff_options{.initial_delay = 1ms, .maximum_delay = 100ms}, [&]() {
        ++attempt_count;
        if (attempt_count < 3) throw retriable_error("not ready yet");
        return 42;
      });
  EXPECT_EQ(attempt_count, 3);
  EXPECT_EQ(result, 42);
}

TEST(BackoffTest, ThrowsRetryExceededErrorOnTimeout) {
  int attempt_count = 0;
  EXPECT_THAT(
      [&]() {
        backoff(backoff_options{.initial_delay = 5ms, .deadline = 20ms}, [&]() {
          ++attempt_count;
          throw retriable_error("resource unavailable");
        });
      },
      ThrowsMessage<retry_exceeded_error>(HasSubstr(
          "Backoff exceeded, aborting. Last retry message: resource "
          "unavailable")));
  EXPECT_GT(attempt_count, 1);
}

TEST(BackoffTest, PropagatesNonRetryExceptionsImmediately) {
  int attempt_count = 0;
  EXPECT_THAT(
      [&]() {
        backoff([&]() {
          ++attempt_count;
          throw std::runtime_error("fatal error");
        });
      },
      ThrowsMessage<std::runtime_error>(HasSubstr("fatal error")));
  EXPECT_EQ(attempt_count, 1);
}

TEST(BackoffTest, ThrowsImmediatelyWhenAlreadyCancelled) {
  cancellation cancel;
  cancel.cancel();

  int attempt_count = 0;
  EXPECT_THAT(
      [&]() {
        backoff(backoff_options{.cancel = cancel}, [&]() { ++attempt_count; });
      },
      ThrowsMessage<retry_cancelled_error>(HasSubstr("Backoff cancelled.")));
  EXPECT_EQ(attempt_count, 0);
}

TEST(BackoffTest, ThrowsImmediatelyWhenCancelledDuringAttempt) {
  cancellation cancel;
  int attempt_count = 0;
  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();

  EXPECT_THAT(
      [&]() {
        backoff(
            backoff_options{
                .initial_delay = 10s,
                .maximum_delay = 10s,
                .deadline = 30s,
                .cancel = cancel},
            [&]() {
              ++attempt_count;
              cancel.cancel();
              throw retriable_error("cancelled mid-attempt");
            });
      },
      ThrowsMessage<retry_cancelled_error>(HasSubstr("Backoff cancelled.")));

  EXPECT_EQ(attempt_count, 1);
  EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(BackoffTest, WakesImmediatelyFromLongDelayWhenCancelledFromAnotherThread) {
  cancellation cancel;
  std::promise<void> first_attempt_failed;
  std::future<void> ready = first_attempt_failed.get_future();

  std::jthread canceller([&]() {
    ready.wait();
    std::this_thread::sleep_for(10ms);
    cancel.cancel();
  });

  int attempt_count = 0;
  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  EXPECT_THAT(
      [&]() {
        backoff(
            backoff_options{
                .initial_delay = 10s,
                .maximum_delay = 10s,
                .deadline = 30s,
                .cancel = cancel},
            [&]() {
              if (++attempt_count == 1) first_attempt_failed.set_value();
              throw retriable_error("still waiting");
            });
      },
      ThrowsMessage<retry_cancelled_error>(HasSubstr("Backoff cancelled.")));

  EXPECT_EQ(attempt_count, 1);
  EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(BackoffTest, SupportsMaxDurationDeadlineWithoutOverflow) {
  cancellation cancel;
  int attempt_count = 0;
  int result = backoff(
      backoff_options{
          .initial_delay = 1ms,
          .maximum_delay = 5ms,
          .deadline = std::chrono::steady_clock::duration::max(),
          .cancel = cancel},
      [&]() {
        if (++attempt_count < 3) throw retriable_error("try again");
        return 99;
      });
  EXPECT_EQ(attempt_count, 3);
  EXPECT_EQ(result, 99);
}

TEST(PollTest, SucceedsWhenCallbackStopsThrowingRetriableError) {
  system_clock::time_point current_time{};
  const cancellation cancel;
  std::vector<system_clock::duration> sleeps;
  int attempt_count = 0;

  int result = poll(
      {.interval = 1s,
       .cancel = cancel,
       .deadline = current_time + 10s,
       .now = [&]() { return current_time; },
       .sleep =
           [&](system_clock::duration duration) {
             sleeps.push_back(duration);
             current_time += duration;
           }},
      [&]() {
        if (++attempt_count < 3) throw retriable_error{};
        return 7;
      });

  EXPECT_EQ(result, 7);
  EXPECT_EQ(attempt_count, 3);
  ASSERT_EQ(sleeps.size(), 2u);
  EXPECT_EQ(sleeps[0], 1s);
  EXPECT_EQ(sleeps[1], 1s);
}

TEST(PollTest, ClampsFinalSleepToRemainingDurationAndThrowsOnDeadline) {
  system_clock::time_point current_time{};
  const system_clock::time_point deadline = current_time + 1250ms;
  std::vector<system_clock::duration> sleeps;

  EXPECT_THAT(
      [&]() {
        poll(
            {.interval = 1s,
             .deadline = deadline,
             .now = [&]() { return current_time; },
             .sleep =
                 [&](system_clock::duration duration) {
                   sleeps.push_back(duration);
                   current_time += duration;
                 }},
            [&]() { throw retriable_error("still lagging"); });
      },
      ThrowsMessage<retry_exceeded_error>(HasSubstr(
          "Poll deadline exceeded, aborting. Last retry message: still "
          "lagging")));

  ASSERT_EQ(sleeps.size(), 2u);
  EXPECT_EQ(sleeps[0], 1s);
  EXPECT_EQ(sleeps[1], 250ms);
  EXPECT_EQ(current_time, deadline);
}

TEST(PollTest, NoCallbackOverloadWaitsUntilDeadline) {
  system_clock::time_point current_time{};
  const system_clock::time_point deadline = current_time + 2s;
  const cancellation cancel;
  std::vector<system_clock::duration> sleeps;

  EXPECT_THROW(
      poll({
          .interval = 1s,
          .cancel = cancel,
          .deadline = deadline,
          .now = [&]() { return current_time; },
          .sleep =
              [&](system_clock::duration duration) {
                sleeps.push_back(duration);
                current_time += duration;
              },
      }),
      retry_exceeded_error);

  ASSERT_EQ(sleeps.size(), 2u);
  EXPECT_EQ(sleeps[0], 1s);
  EXPECT_EQ(sleeps[1], 1s);
  EXPECT_EQ(current_time, deadline);
}

TEST(PollTest, ThrowsImmediatelyWhenStartedPastDeadline) {
  const system_clock::time_point current_time =
      system_clock::time_point{} + 10s;
  const system_clock::time_point deadline = system_clock::time_point{} + 5s;
  int attempt_count = 0;
  int sleep_count = 0;

  EXPECT_THROW(
      poll(
          {.interval = 1s,
           .deadline = deadline,
           .now = [&]() { return current_time; },
           .sleep = [&](system_clock::duration) { ++sleep_count; }},
          [&]() { ++attempt_count; }),
      retry_exceeded_error);

  EXPECT_EQ(attempt_count, 0);
  EXPECT_EQ(sleep_count, 0);
}

TEST(PollTest, ThrowsRetryCancelledErrorWhenCancelled) {
  cancellation cancel;
  std::promise<void> first_attempt_failed;
  std::future<void> ready = first_attempt_failed.get_future();

  std::jthread canceller([&]() {
    ready.wait();
    std::this_thread::sleep_for(10ms);
    cancel.cancel();
  });

  int attempt_count = 0;
  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  EXPECT_THAT(
      [&]() {
        poll(
            {.interval = 10s,
             .cancel = cancel,
             .deadline = system_clock::now() + 30s},
            [&]() {
              if (++attempt_count == 1) first_attempt_failed.set_value();
              throw retriable_error{};
            });
      },
      ThrowsMessage<retry_cancelled_error>(HasSubstr("Poll cancelled.")));

  EXPECT_EQ(attempt_count, 1);
  EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(PollTest, RejectsInvalidOptions) {
  EXPECT_THROW(poll({.interval = 0s}, []() {}), std::invalid_argument);
  EXPECT_THROW(poll({.interval = -1s}, []() {}), std::invalid_argument);
  EXPECT_THROW(
      poll({.interval = 1s, .now = nullptr}, []() {}), std::invalid_argument);
}

} // namespace
} // namespace howling
