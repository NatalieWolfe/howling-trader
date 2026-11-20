#include "functional/retry.h"

#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>

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
        if (attempt_count < 3) throw retry("not ready yet");
        return 42;
      });
  EXPECT_EQ(attempt_count, 3);
  EXPECT_EQ(result, 42);
}

TEST(BackoffTest, ThrowsBackoffExceededErrorOnTimeout) {
  int attempt_count = 0;
  EXPECT_THAT(
      [&]() {
        backoff(
            backoff_options{.initial_delay = 5ms, .maximum_delay = 20ms},
            [&]() {
              ++attempt_count;
              throw retry("resource unavailable");
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

} // namespace
} // namespace howling
