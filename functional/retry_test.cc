#include "functional/retry.h"

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

#include "signaling/cancellation.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

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

TEST(BackoffTest, ThrowsBackoffExceededErrorOnTimeout) {
  int attempt_count = 0;
  EXPECT_THAT(
      [&]() {
        backoff(backoff_options{.initial_delay = 5ms, .deadline = 20ms}, [&]() {
          ++attempt_count;
          throw retriable_error("resource unavailable");
        });
      },
      ThrowsMessage<backoff_exceeded_error>(HasSubstr(
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
      ThrowsMessage<backoff_cancelled_error>(HasSubstr("Backoff cancelled.")));
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
      ThrowsMessage<backoff_cancelled_error>(HasSubstr("Backoff cancelled.")));

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
      ThrowsMessage<backoff_cancelled_error>(HasSubstr("Backoff cancelled.")));

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

} // namespace
} // namespace howling
