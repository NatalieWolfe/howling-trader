#include "signaling/signal_handler.h"

#include <atomic>
#include <csignal>
#include <stdexcept>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace howling {
namespace {

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

TEST(SignalHandlerTest, UninitializedStateAndSingleInitialization) {
  ASSERT_FALSE(signal_handler::is_initialized());
  EXPECT_THROW(static_cast<void>(signal_handler::instance()), std::logic_error);

  signal_handler::initialize();

  EXPECT_TRUE(signal_handler::is_initialized());
  signal_handler& handler = signal_handler::instance();
  EXPECT_FALSE(handler.has_received_signal());
  EXPECT_EQ(handler.received_signal(), 0);

  // Initializing a second time must fail rather than overwrite state.
  EXPECT_THROW(signal_handler::initialize(), std::logic_error);
}

TEST(SignalHandlerTest, CapturesSigtermAndResets) {
  ensure_initialized();
  signal_reset_guard guard;
  signal_handler& handler = signal_handler::instance();

  ASSERT_FALSE(handler.has_received_signal());
  ASSERT_EQ(handler.received_signal(), 0);

  ASSERT_EQ(std::raise(SIGTERM), 0);
  EXPECT_TRUE(handler.has_received_signal());
  EXPECT_EQ(handler.received_signal(), SIGTERM);

  handler.reset_signal();
  EXPECT_FALSE(handler.has_received_signal());
  EXPECT_EQ(handler.received_signal(), 0);
}

TEST(SignalHandlerTest, CapturesSigintAndResets) {
  ensure_initialized();
  signal_reset_guard guard;
  signal_handler& handler = signal_handler::instance();

  ASSERT_EQ(std::raise(SIGINT), 0);
  EXPECT_TRUE(handler.has_received_signal());
  EXPECT_EQ(handler.received_signal(), SIGINT);

  handler.reset_signal();
  EXPECT_FALSE(handler.has_received_signal());
  EXPECT_EQ(handler.received_signal(), 0);
}

TEST(SignalHandlerTest, PreservesFirstReceivedSignal) {
  ensure_initialized();
  signal_reset_guard guard;
  signal_handler& handler = signal_handler::instance();

  ASSERT_EQ(std::raise(SIGTERM), 0);
  ASSERT_EQ(std::raise(SIGINT), 0);
  EXPECT_TRUE(handler.has_received_signal());
  EXPECT_EQ(handler.received_signal(), SIGTERM);
}

TEST(SignalHandlerTest, ConcurrentAccessAndRepeatedInitAreThreadSafe) {
  ensure_initialized();
  signal_reset_guard guard;
  signal_handler& handler = signal_handler::instance();

  constexpr int thread_count = 4;
  std::atomic<int> ready_threads{0};
  std::atomic<int> observed_sigterm_count{0};
  std::atomic<int> rejected_init_count{0};

  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (int i = 0; i < thread_count; ++i) {
    threads.emplace_back([&]() {
      try {
        signal_handler::initialize();
      } catch (const std::logic_error&) {
        rejected_init_count.fetch_add(1, std::memory_order_relaxed);
      }

      ready_threads.fetch_add(1, std::memory_order_release);
      while (!handler.has_received_signal()) std::this_thread::yield();
      if (handler.received_signal() == SIGTERM) {
        observed_sigterm_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  while (ready_threads.load(std::memory_order_acquire) < thread_count) {
    std::this_thread::yield();
  }
  ASSERT_EQ(std::raise(SIGTERM), 0);

  for (std::thread& worker : threads) worker.join();

  EXPECT_EQ(rejected_init_count.load(), thread_count);
  EXPECT_EQ(observed_sigterm_count.load(), thread_count);
}

} // namespace
} // namespace howling
