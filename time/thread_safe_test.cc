#include "time/thread_safe.h"

#include <atomic>
#include <chrono>
#include <compare>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace howling {
namespace {

using ::std::chrono::milliseconds;
using ::std::chrono::seconds;
using ::std::chrono::system_clock;

struct state_holder {
  int identifier;
  atomic_time_point time_now{};
};

state_holder make_holder(int identifier, system_clock::time_point timestamp) {
  state_holder result{.identifier = identifier, .time_now = timestamp};
  return result;
}

system_clock::time_point
pass_through_time_point(system_clock::time_point value) {
  return value;
}

TEST(AtomicTimePointTest, DefaultConstructsToEpoch) {
  constexpr atomic_time_point default_timestamp;
  EXPECT_EQ(default_timestamp.load(), system_clock::time_point{});
  EXPECT_EQ(default_timestamp.load().time_since_epoch().count(), 0);
}

TEST(AtomicTimePointTest, ConstructsFromTimePoint) {
  constexpr system_clock::time_point epoch{};
  constexpr atomic_time_point from_epoch{epoch};
  EXPECT_EQ(from_epoch.load(), epoch);

  const system_clock::time_point positive_time = epoch + seconds{12345};
  const atomic_time_point positive_atomic = positive_time;
  EXPECT_EQ(positive_atomic.load(), positive_time);

  const system_clock::time_point negative_time = epoch - seconds{54321};
  const atomic_time_point negative_atomic{negative_time};
  EXPECT_EQ(negative_atomic.load(), negative_time);
}

TEST(AtomicTimePointTest, CopyAndMoveConstructorsAndAssignments) {
  const system_clock::time_point first_time =
      system_clock::time_point{} + seconds{100};
  const system_clock::time_point second_time =
      system_clock::time_point{} + seconds{200};
  const system_clock::time_point third_time =
      system_clock::time_point{} + seconds{300};

  atomic_time_point original{first_time};
  atomic_time_point copy_constructed{original};
  EXPECT_EQ(copy_constructed.load(), first_time);

  // Mutating original must not affect the copy.
  original.store(second_time);
  EXPECT_EQ(copy_constructed.load(), first_time);
  EXPECT_EQ(original.load(), second_time);

  atomic_time_point move_constructed{std::move(original)};
  EXPECT_EQ(move_constructed.load(), second_time);

  atomic_time_point copy_assigned;
  copy_assigned = copy_constructed;
  EXPECT_EQ(copy_assigned.load(), first_time);

  // Self-assignment must leave the stored value intact.
  const atomic_time_point& self_reference = copy_assigned;
  copy_assigned = self_reference;
  EXPECT_EQ(copy_assigned.load(), first_time);

  atomic_time_point move_assigned;
  move_assigned = std::move(move_constructed);
  EXPECT_EQ(move_assigned.load(), second_time);

  // Assignment directly from time_point supports chaining.
  atomic_time_point chained_first;
  atomic_time_point chained_second;
  chained_first = chained_second = third_time;
  EXPECT_EQ(chained_first.load(), third_time);
  EXPECT_EQ(chained_second.load(), third_time);
}

TEST(AtomicTimePointTest, LoadStoreAndExchange) {
  const system_clock::time_point first_time =
      system_clock::time_point{} + milliseconds{1000};
  const system_clock::time_point second_time =
      system_clock::time_point{} + milliseconds{2500};
  const system_clock::time_point third_time =
      system_clock::time_point{} + milliseconds{5000};

  atomic_time_point timestamp;
  timestamp.store(first_time);
  EXPECT_EQ(timestamp.load(), first_time);

  timestamp.store(second_time, std::memory_order_release);
  EXPECT_EQ(timestamp.load(std::memory_order_acquire), second_time);

  const system_clock::time_point previous =
      timestamp.exchange(third_time, std::memory_order_acq_rel);
  EXPECT_EQ(previous, second_time);
  EXPECT_EQ(timestamp.load(std::memory_order_relaxed), third_time);
}

TEST(AtomicTimePointTest, ImplicitConversionAndArithmetic) {
  const system_clock::time_point base_time =
      system_clock::time_point{} + seconds{1000};
  const system_clock::time_point current_time = base_time + seconds{42};
  const atomic_time_point atomic_base{base_time};
  const atomic_time_point atomic_current{current_time};

  const system_clock::time_point converted = atomic_base;
  EXPECT_EQ(converted, base_time);
  EXPECT_EQ(pass_through_time_point(atomic_base), base_time);

  EXPECT_EQ(current_time - atomic_base, seconds{42});
  EXPECT_EQ(atomic_current - base_time, seconds{42});
  EXPECT_EQ(atomic_current - atomic_base, seconds{42});

  EXPECT_EQ(atomic_base + seconds{42}, current_time);
  EXPECT_EQ(seconds{42} + atomic_base, current_time);
  EXPECT_EQ(atomic_current - seconds{42}, base_time);
}

TEST(AtomicTimePointTest, ComparisonOperators) {
  const system_clock::time_point earlier_time =
      system_clock::time_point{} + seconds{10};
  const system_clock::time_point later_time =
      system_clock::time_point{} + seconds{20};

  const atomic_time_point earlier{earlier_time};
  const atomic_time_point equal_to_earlier{earlier_time};
  const atomic_time_point later{later_time};

  EXPECT_EQ(earlier, equal_to_earlier);
  EXPECT_NE(earlier, later);
  EXPECT_LT(earlier, later);
  EXPECT_LE(earlier, equal_to_earlier);
  EXPECT_GT(later, earlier);
  EXPECT_GE(later, earlier);
  EXPECT_EQ(earlier <=> later, std::strong_ordering::less);
  EXPECT_EQ(earlier <=> equal_to_earlier, std::strong_ordering::equal);
  EXPECT_EQ(later <=> earlier, std::strong_ordering::greater);

  // Verify comparisons with system_clock::time_point in both operand orders.
  EXPECT_EQ(earlier, earlier_time);
  EXPECT_EQ(earlier_time, earlier);
  EXPECT_NE(earlier, later_time);
  EXPECT_NE(later_time, earlier);
  EXPECT_LT(earlier, later_time);
  EXPECT_LT(earlier_time, later);
  EXPECT_GT(later, earlier_time);
  EXPECT_GT(later_time, earlier);
}

TEST(AtomicTimePointTest, AggregateDesignatedInitializersAndReturnByValue) {
  static_assert(std::is_aggregate_v<state_holder>);
  static_assert(std::is_copy_constructible_v<state_holder>);
  static_assert(std::is_move_constructible_v<state_holder>);
  static_assert(std::is_copy_assignable_v<state_holder>);
  static_assert(std::is_move_assignable_v<state_holder>);

  const state_holder defaulted{.identifier = 1};
  EXPECT_EQ(defaulted.identifier, 1);
  EXPECT_EQ(defaulted.time_now.load(), system_clock::time_point{});

  const system_clock::time_point expected_time =
      system_clock::time_point{} + seconds{777};
  const state_holder initialized{
      .identifier = 2,
      .time_now = expected_time,
  };
  EXPECT_EQ(initialized.identifier, 2);
  EXPECT_EQ(initialized.time_now.load(), expected_time);

  const state_holder returned = make_holder(3, expected_time);
  EXPECT_EQ(returned.identifier, 3);
  EXPECT_EQ(returned.time_now.load(), expected_time);

  std::vector<state_holder> holders;
  holders.push_back(returned);
  holders.push_back(make_holder(4, expected_time + seconds{1}));
  ASSERT_EQ(holders.size(), 2U);
  EXPECT_EQ(holders[0].identifier, 3);
  EXPECT_EQ(holders[0].time_now.load(), expected_time);
  EXPECT_EQ(holders[1].identifier, 4);
  EXPECT_EQ(holders[1].time_now.load(), expected_time + seconds{1});
}

TEST(AtomicTimePointTest, ConcurrentReaderWriterStressTest) {
  constexpr int reader_count = 4;
  constexpr int iteration_count = 20000;

  const system_clock::time_point base_time =
      system_clock::time_point{} + seconds{1000000};
  const system_clock::time_point final_time =
      base_time + milliseconds{iteration_count};

  atomic_time_point shared_time{base_time};
  std::atomic<int> ready_readers{0};
  std::atomic<bool> writer_done{false};
  std::atomic<bool> invariant_violated{false};

  std::vector<std::thread> readers;
  readers.reserve(reader_count);
  for (int i = 0; i < reader_count; ++i) {
    readers.emplace_back([&]() {
      system_clock::time_point previous = base_time;
      ready_readers.fetch_add(1, std::memory_order_release);
      while (!writer_done.load(std::memory_order_acquire)) {
        const system_clock::time_point observed = shared_time;
        if (observed < previous || observed > final_time) {
          invariant_violated.store(true, std::memory_order_relaxed);
        }
        previous = observed;
      }
      const system_clock::time_point last_observed = shared_time.load();
      if (last_observed < previous || last_observed != final_time) {
        invariant_violated.store(true, std::memory_order_relaxed);
      }
    });
  }

  std::thread writer([&]() {
    while (ready_readers.load(std::memory_order_acquire) < reader_count) {
      std::this_thread::yield();
    }
    for (int i = 1; i <= iteration_count; ++i) {
      shared_time.store(base_time + milliseconds{i}, std::memory_order_release);
    }
    writer_done.store(true, std::memory_order_release);
  });

  writer.join();
  for (std::thread& reader : readers) { reader.join(); }

  EXPECT_FALSE(invariant_violated.load(std::memory_order_acquire));
  EXPECT_EQ(shared_time.load(), final_time);
}

} // namespace
} // namespace howling
