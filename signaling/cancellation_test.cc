#include "signaling/cancellation.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <optional>
#include <thread>
#include <utility>

#include "logs/testing/log_capture.h"
#include "signaling/signal_handler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using namespace std::chrono_literals;

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

TEST(CancellationTest, SupportsInPlaceChildConstruction) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation parent;
  std::optional<cancellation> child;
  child.emplace(parent);

  EXPECT_FALSE(child->is_cancelled());
  parent.cancel();
  EXPECT_TRUE(child->is_cancelled());
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

TEST(CancellationTest, WaitForReturnsFalseOnTimeoutWhenNotCancelled) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  EXPECT_FALSE(const_ref(token).wait_for(5ms));
  EXPECT_FALSE(token.is_cancelled());
}

TEST(CancellationTest, WaitForWakesImmediatelyOnCancelFromAnotherThread) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  std::atomic<bool> waiter_entering{false};
  std::atomic<bool> wait_result{false};

  auto start = std::chrono::steady_clock::now();
  std::thread waiter([&]() {
    waiter_entering.store(true, std::memory_order_release);
    wait_result.store(
        const_ref(token).wait_for(10s), std::memory_order_release);
  });

  while (!waiter_entering.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  std::this_thread::sleep_for(10ms);
  token.cancel();
  waiter.join();
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_TRUE(wait_result.load(std::memory_order_acquire));
  EXPECT_LT(elapsed, 2s);
}

TEST(CancellationTest, ChildWaitForWakesImmediatelyWhenParentCancelled) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation parent;
  cancellation child(parent);
  std::atomic<bool> waiter_entering{false};
  std::atomic<bool> wait_result{false};

  auto start = std::chrono::steady_clock::now();
  std::thread waiter([&]() {
    waiter_entering.store(true, std::memory_order_release);
    wait_result.store(
        const_ref(child).wait_for(10s), std::memory_order_release);
  });

  while (!waiter_entering.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  std::this_thread::sleep_for(10ms);
  parent.cancel();
  waiter.join();
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_TRUE(wait_result.load(std::memory_order_acquire));
  EXPECT_LT(elapsed, 2s);
}

TEST(CancellationTest, WaitForWakesPromptlyOnProcessSignal) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  std::atomic<bool> waiter_entering{false};
  std::atomic<bool> wait_result{false};

  auto start = std::chrono::steady_clock::now();
  std::thread waiter([&]() {
    waiter_entering.store(true, std::memory_order_release);
    wait_result.store(
        const_ref(token).wait_for(10s), std::memory_order_release);
  });

  while (!waiter_entering.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  std::this_thread::sleep_for(10ms);
  ASSERT_EQ(std::raise(SIGTERM), 0);
  waiter.join();
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_TRUE(wait_result.load(std::memory_order_acquire));
  EXPECT_LT(elapsed, 2s);
}

TEST(CancellationTest, OnCancelFiresImmediatelyWhenAlreadyCancelled) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  token.cancel();

  int call_count = 0;
  auto reg = const_ref(token).on_cancel([&]() { ++call_count; });
  EXPECT_EQ(call_count, 1);
  reg.unregister();
  EXPECT_EQ(call_count, 1);
}

TEST(CancellationTest, OnCancelFiresOnCancelAndParentCancelFromAnotherThread) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation parent;
  cancellation child(parent);
  std::atomic<int> parent_cb_count{0};
  std::atomic<int> child_cb_count{0};

  auto parent_reg = parent.on_cancel(
      [&]() { parent_cb_count.fetch_add(1, std::memory_order_relaxed); });
  auto child_reg = const_ref(child).on_cancel(
      [&]() { child_cb_count.fetch_add(1, std::memory_order_relaxed); });

  std::thread canceller([&]() { parent.cancel(); });
  canceller.join();

  EXPECT_EQ(parent_cb_count.load(std::memory_order_relaxed), 1);
  EXPECT_EQ(child_cb_count.load(std::memory_order_relaxed), 1);
}

TEST(CancellationTest, OnCancelDoesNotFireAfterUnregisterOrDestruction) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  int unregistered_count = 0;
  int scoped_count = 0;
  int active_count = 0;

  auto reg1 = token.on_cancel([&]() { ++unregistered_count; });
  reg1.unregister();

  {
    auto reg2 = token.on_cancel([&]() { ++scoped_count; });
  }

  auto reg3 = token.on_cancel([&]() { ++active_count; });
  cancellation::callback_registration moved_reg = std::move(reg3);

  token.cancel();
  EXPECT_EQ(unregistered_count, 0);
  EXPECT_EQ(scoped_count, 0);
  EXPECT_EQ(active_count, 1);
}

} // namespace
} // namespace howling
