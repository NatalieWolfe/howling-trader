#include "signaling/cancellation.h"

#include <atomic>
#include <csignal>
#include <thread>

#include "logs/testing/log_capture.h"
#include "signaling/signal_handler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

void ensure_initialized() {
  if (!signal_handler::is_initialized()) signal_handler::initialize();
}

class signal_reset_guard {
public:
  signal_reset_guard() { signal_handler::instance().reset_signal(); }
  ~signal_reset_guard() { signal_handler::instance().reset_signal(); }

  signal_reset_guard(const signal_reset_guard&) = delete;
  signal_reset_guard& operator=(const signal_reset_guard&) = delete;
};

const cancellation& const_ref(const cancellation& handle) {
  return handle;
}

TEST(CancellationTest, WorksAndLogsWarningWhenSignalHandlerUninitialized) {
  ASSERT_FALSE(signal_handler::is_initialized());
  testing::log_capture capture;

  cancellation token;
  EXPECT_THAT(
      capture.warnings(),
      ElementsAre(HasSubstr("signal_handler::initialize()")));

  EXPECT_FALSE(const_ref(token).is_cancelled());
  EXPECT_EQ(token.received_signal(), 0);

  token.cancel();
  EXPECT_TRUE(const_ref(token).is_cancelled());
  EXPECT_EQ(token.received_signal(), 0);
}

TEST(CancellationTest, ParentCancellationPropagatesBeforeInitialization) {
  ASSERT_FALSE(signal_handler::is_initialized());

  cancellation parent;
  cancellation first_child = cancellation::with_parent(parent);

  first_child.cancel();
  EXPECT_TRUE(first_child.is_cancelled());
  EXPECT_FALSE(parent.is_cancelled());

  cancellation second_child = cancellation::with_parent(parent);
  EXPECT_FALSE(second_child.is_cancelled());

  parent.cancel();
  EXPECT_TRUE(parent.is_cancelled());
  EXPECT_TRUE(second_child.is_cancelled());
}

TEST(CancellationTest, ObservesProcessSignalsAfterSignalHandlerInitialized) {
  ensure_initialized();
  signal_reset_guard guard;
  testing::log_capture capture;

  cancellation token;
  EXPECT_THAT(capture.warnings(), IsEmpty());
  EXPECT_FALSE(const_ref(token).is_cancelled());
  EXPECT_EQ(token.received_signal(), 0);

  ASSERT_EQ(std::raise(SIGTERM), 0);
  EXPECT_TRUE(const_ref(token).is_cancelled());
  EXPECT_EQ(token.received_signal(), SIGTERM);

  signal_handler::instance().reset_signal();
  EXPECT_FALSE(const_ref(token).is_cancelled());
  EXPECT_EQ(token.received_signal(), 0);
}

TEST(CancellationTest, IndependentHandlesAreIsolated) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation first;
  cancellation second;

  first.cancel();
  EXPECT_TRUE(first.is_cancelled());
  EXPECT_FALSE(second.is_cancelled());
  EXPECT_EQ(first.received_signal(), 0);
  EXPECT_EQ(second.received_signal(), 0);
}

TEST(CancellationTest, ChildObservesParentAndProcessSignals) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation parent;
  cancellation scoped_child = cancellation::with_parent(parent);

  scoped_child.cancel();
  EXPECT_TRUE(scoped_child.is_cancelled());
  EXPECT_FALSE(parent.is_cancelled());

  cancellation propagated_child = cancellation::with_parent(parent);
  parent.cancel();
  EXPECT_TRUE(propagated_child.is_cancelled());

  cancellation signal_parent;
  cancellation signal_child = cancellation::with_parent(signal_parent);
  ASSERT_EQ(std::raise(SIGINT), 0);
  EXPECT_TRUE(signal_parent.is_cancelled());
  EXPECT_TRUE(signal_child.is_cancelled());
  EXPECT_EQ(signal_parent.received_signal(), SIGINT);
  EXPECT_EQ(signal_child.received_signal(), SIGINT);
}

TEST(CancellationTest, ConcurrentCancelAndPollIsThreadSafe) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  const cancellation& observer = token;
  std::atomic<bool> worker_started{false};
  std::atomic<bool> worker_observed_cancel{false};

  std::thread worker([&]() {
    worker_started.store(true, std::memory_order_release);
    while (!observer.is_cancelled()) std::this_thread::yield();
    worker_observed_cancel.store(true, std::memory_order_release);
  });

  while (!worker_started.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  token.cancel();
  worker.join();

  EXPECT_TRUE(worker_observed_cancel.load(std::memory_order_acquire));
}

} // namespace
} // namespace howling
