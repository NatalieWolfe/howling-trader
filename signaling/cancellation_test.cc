#include "signaling/cancellation.h"

#include <atomic>
#include <csignal>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "signaling/signal_handler.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

class warning_log_capture : public absl::LogSink {
public:
  warning_log_capture() { absl::AddLogSink(this); }
  ~warning_log_capture() override { absl::RemoveLogSink(this); }

  warning_log_capture(const warning_log_capture&) = delete;
  warning_log_capture& operator=(const warning_log_capture&) = delete;

  void Send(const absl::LogEntry& entry) override {
    if (entry.log_severity() == absl::LogSeverity::kWarning) {
      std::lock_guard<std::mutex> lock(_mutex);
      _warnings.emplace_back(entry.text_message());
    }
  }

  [[nodiscard]] std::vector<std::string> warnings() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _warnings;
  }

private:
  mutable std::mutex _mutex;
  std::vector<std::string> _warnings;
};

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

bool is_cancelled_via_const_ref(const cancellation& handle) {
  return handle.is_cancelled();
}

TEST(CancellationTest, WorksAndLogsWarningWhenSignalHandlerUninitialized) {
  ASSERT_FALSE(signal_handler::is_initialized());
  warning_log_capture capture;

  cancellation token;
  std::vector<std::string> logged_warnings = capture.warnings();
  ASSERT_EQ(logged_warnings.size(), 1u);
  EXPECT_NE(
      logged_warnings.front().find("signal_handler::initialize()"),
      std::string::npos);

  EXPECT_FALSE(is_cancelled_via_const_ref(token));
  EXPECT_EQ(token.received_signal(), 0);

  token.cancel();
  EXPECT_TRUE(is_cancelled_via_const_ref(token));
  EXPECT_EQ(token.received_signal(), 0);
}

TEST(CancellationTest, ParentCancellationPropagatesBeforeInitialization) {
  ASSERT_FALSE(signal_handler::is_initialized());

  cancellation parent;
  cancellation first_child{&parent};

  first_child.cancel();
  EXPECT_TRUE(first_child.is_cancelled());
  EXPECT_FALSE(parent.is_cancelled());

  cancellation second_child{&parent};
  EXPECT_FALSE(second_child.is_cancelled());

  parent.cancel();
  EXPECT_TRUE(parent.is_cancelled());
  EXPECT_TRUE(second_child.is_cancelled());
}

TEST(CancellationTest, ObservesProcessSignalsAfterSignalHandlerInitialized) {
  ensure_initialized();
  signal_reset_guard guard;
  warning_log_capture capture;

  cancellation token;
  EXPECT_TRUE(capture.warnings().empty());
  EXPECT_FALSE(is_cancelled_via_const_ref(token));
  EXPECT_EQ(token.received_signal(), 0);

  ASSERT_EQ(std::raise(SIGTERM), 0);
  EXPECT_TRUE(is_cancelled_via_const_ref(token));
  EXPECT_EQ(token.received_signal(), SIGTERM);

  signal_handler::instance().reset_signal();
  EXPECT_FALSE(is_cancelled_via_const_ref(token));
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
  cancellation scoped_child{&parent};

  scoped_child.cancel();
  EXPECT_TRUE(scoped_child.is_cancelled());
  EXPECT_FALSE(parent.is_cancelled());

  cancellation propagated_child{&parent};
  parent.cancel();
  EXPECT_TRUE(propagated_child.is_cancelled());

  cancellation signal_parent;
  cancellation signal_child{&signal_parent};
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
