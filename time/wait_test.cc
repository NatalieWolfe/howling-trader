#include "time/wait.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "signaling/cancellation.h"
#include "signaling/signal_handler.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

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

TEST(WaitTest, WaitForReturnsCompletedOnTimeoutWhenNotCancelled) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  EXPECT_EQ(wait_for(5ms, const_ref(token)), wait_result::COMPLETED);
  EXPECT_FALSE(token.is_cancelled());
}

TEST(WaitTest, WaitForWakesImmediatelyOnCancelFromAnotherThread) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  std::atomic<bool> waiter_entering{false};
  std::atomic<wait_result> result{wait_result::COMPLETED};

  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  std::thread waiter([&]() {
    waiter_entering.store(true, std::memory_order_release);
    result.store(wait_for(10s, const_ref(token)), std::memory_order_release);
  });

  while (!waiter_entering.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  std::this_thread::sleep_for(10ms);
  token.cancel();
  waiter.join();
  const std::chrono::steady_clock::duration elapsed =
      std::chrono::steady_clock::now() - start;

  EXPECT_EQ(result.load(std::memory_order_acquire), wait_result::CANCELLED);
  EXPECT_LT(elapsed, 2s);
}

TEST(WaitTest, ChildWaitForWakesImmediatelyWhenParentCancelled) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation parent;
  cancellation child = cancellation::with_parent(parent);
  std::atomic<bool> waiter_entering{false};
  std::atomic<wait_result> result{wait_result::COMPLETED};

  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  std::thread waiter([&]() {
    waiter_entering.store(true, std::memory_order_release);
    result.store(wait_for(10s, const_ref(child)), std::memory_order_release);
  });

  while (!waiter_entering.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  std::this_thread::sleep_for(10ms);
  parent.cancel();
  waiter.join();
  const std::chrono::steady_clock::duration elapsed =
      std::chrono::steady_clock::now() - start;

  EXPECT_EQ(result.load(std::memory_order_acquire), wait_result::CANCELLED);
  EXPECT_LT(elapsed, 2s);
}

TEST(WaitTest, WaitForReturnsCancelledWhenProcessSignalReceived) {
  ensure_initialized();
  signal_reset_guard guard;

  cancellation token;
  ASSERT_EQ(std::raise(SIGTERM), 0);

  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  EXPECT_EQ(wait_for(10s, const_ref(token)), wait_result::CANCELLED);
  const std::chrono::steady_clock::duration elapsed =
      std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, 2s);
}

} // namespace
} // namespace howling
