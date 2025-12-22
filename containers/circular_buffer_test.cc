#include "containers/circular_buffer.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "gtest/gtest.h"

namespace howling {
namespace {

using namespace std::chrono_literals;

TEST(CircularBuffer, Construction) {
  circular_buffer<int> foo{3};
  EXPECT_EQ(foo.size(), 0);
  EXPECT_EQ(foo.capacity(), 3);
  EXPECT_TRUE(foo.empty());
}

TEST(CircularBuffer, PushBack) {
  circular_buffer<int> foo{3};
  foo.push_back(1);
  EXPECT_EQ(foo.size(), 1);
  EXPECT_EQ(foo.front(), 1);
  EXPECT_EQ(foo.back(), 1);
  EXPECT_FALSE(foo.empty());

  foo.push_back(2);
  EXPECT_EQ(foo.size(), 2);
  EXPECT_EQ(foo.front(), 1);
  EXPECT_EQ(foo.back(), 2);
}

TEST(CircularBuffer, PopFront) {
  circular_buffer<int> foo{3};
  foo.push_back(1);
  foo.push_back(2);
  foo.push_back(3);

  EXPECT_EQ(foo.size(), 3);
  EXPECT_EQ(foo.front(), 1);

  foo.pop_front();
  EXPECT_EQ(foo.size(), 2);
  EXPECT_EQ(foo.front(), 2);
  EXPECT_EQ(foo.back(), 3);

  foo.pop_front();
  foo.pop_front();
  EXPECT_TRUE(foo.empty());

  // Popping from an empty buffer should throw.
  EXPECT_THROW(foo.pop_front(), std::range_error);
}

TEST(CircularBuffer, Accessors) {
  circular_buffer<int> foo{3};
  foo.push_back(1);
  foo.push_back(2);
  foo.push_back(3);
  foo.push_back(4); // Overwrites 1

  // front() is 2, back() is 4
  EXPECT_EQ(foo[0], 2);
  EXPECT_EQ(foo[1], 3);
  EXPECT_EQ(foo[2], 4);

  EXPECT_EQ(foo.at(0), 2);
  EXPECT_EQ(foo.at(1), 3);
  EXPECT_EQ(foo.at(2), 4);

  EXPECT_THROW(foo.at(3), std::range_error);

  const auto& const_foo = foo;
  EXPECT_EQ(const_foo.at(0), 2);
  EXPECT_EQ(const_foo.at(1), 3);
  EXPECT_THROW(const_foo.at(3), std::range_error);
}

TEST(CircularBuffer, Clear) {
  circular_buffer<int> foo{5};
  foo.push_back(1);
  foo.push_back(2);
  foo.clear();

  EXPECT_EQ(foo.size(), 0);
  EXPECT_TRUE(foo.empty());
  EXPECT_EQ(foo.begin(), foo.end());

  // Can still push after clearing
  foo.push_back(3);
  EXPECT_EQ(foo.size(), 1);
  EXPECT_EQ(foo.front(), 3);
}

TEST(CircularBuffer, Iterate) {
  circular_buffer<int> foo{3};
  foo.push_back(1);
  foo.push_back(2);
  foo.push_back(3);

  int count = 0;
  for (int elem : foo) ASSERT_EQ(elem, ++count);
  EXPECT_EQ(count, 3);
}

TEST(CircularBuffer, Rollover) {
  circular_buffer<int> foo{3};
  foo.push_back(1);
  foo.push_back(2);
  foo.push_back(3);
  foo.push_back(4);
  foo.push_back(5);
  foo.push_back(6);

  int count = 0;
  for (int elem : foo) ASSERT_EQ(elem, ++count + 3);
  EXPECT_EQ(count, 3);
}

TEST(CircularBuffer, PartialRollover) {
  circular_buffer<int> foo{3};
  foo.push_back(1);
  foo.push_back(2);
  foo.push_back(3);
  foo.push_back(4);
  foo.push_back(5);

  int count = 0;
  for (int elem : foo) ASSERT_EQ(elem, ++count + 2);
  EXPECT_EQ(count, 3);
}

TEST(CircularBuffer, StableIteration) {
  circular_buffer<int> foo{10};
  for (int i = 0; i < foo.capacity(); ++i) foo.push_back(i + 1);
  EXPECT_EQ(foo[0], 1);
  EXPECT_EQ(foo[1], 2);
  EXPECT_EQ(foo[2], 3);
  EXPECT_EQ(foo[3], 4);
  EXPECT_EQ(foo[4], 5);
  EXPECT_EQ(foo[5], 6);
  EXPECT_EQ(foo[6], 7);
  EXPECT_EQ(foo[7], 8);
  EXPECT_EQ(foo[8], 9);
  EXPECT_EQ(foo[9], 10);
  auto itr = foo.begin();
  EXPECT_EQ(*itr, 1);
  for (; *itr <= 10; ++itr) foo.push_back(*itr * 100);
  EXPECT_EQ(*itr, 100);
  EXPECT_EQ(foo[0], 100);
  EXPECT_EQ(foo[1], 200);
  EXPECT_EQ(foo[2], 300);
  EXPECT_EQ(foo[3], 400);
  EXPECT_EQ(foo[4], 500);
  EXPECT_EQ(foo[5], 600);
  EXPECT_EQ(foo[6], 700);
  EXPECT_EQ(foo[7], 800);
  EXPECT_EQ(foo[8], 900);
  EXPECT_EQ(foo[9], 1000);
}

TEST(CircularBuffer, TrailingIteration) {
  circular_buffer<int> foo{100};
  auto itr = foo.begin();
  for (int i = 0; i < 98; ++i) {
    foo.push_back((i * 2) + 0);
    foo.push_back((i * 2) + 1);
    ASSERT_EQ(*itr, i);
    ++itr;
  }
}

TEST(CircularBuffer, ForeverIteration) {
  circular_buffer<int> foo{3};
  int i = 0;
  foo.push_back(i);
  for (int n : foo) {
    ASSERT_EQ(n, i);
    if (++i < 1000) foo.push_back(i);
  }
  EXPECT_GE(i, 1000);
}

TEST(CircularBuffer, TailContinuation) {
  circular_buffer<int> foo{10};
  auto itr = foo.begin();
  int counter = 0;
  foo.push_back(1);
  foo.push_back(2);
  foo.push_back(3);
  while (itr != foo.end()) {
    EXPECT_EQ(*itr, ++counter);
    ++itr;
  }
  foo.push_back(4);
  foo.push_back(5);
  foo.push_back(6);
  while (itr != foo.end()) {
    EXPECT_EQ(*itr, ++counter);
    ++itr;
  }
  EXPECT_EQ(counter, 6);
}

TEST(CircularBuffer, CrossThreadReadWrite) {
  circular_buffer<int> foo{100};
  std::condition_variable signal;
  std::mutex mutex;
  bool ready = false;
  std::thread writer([&]() {
    {
      std::unique_lock lock{mutex};
      signal.wait(lock, [&]() { return ready; });
    }
    for (int i = 1; i <= 50; ++i) foo.push_back(i);
  });

  int counter = 0;
  std::thread reader([&]() {
    {
      std::unique_lock lock{mutex};
      signal.wait(lock, [&]() { return ready; });
    }
    auto itr = foo.begin();
    while (counter < 50) {
      while (itr == foo.end()) { std::this_thread::yield(); }
      EXPECT_EQ(*itr, ++counter);
      ++itr;
    }
  });

  {
    std::lock_guard lock{mutex};
    ready = true;
  }
  signal.notify_all();

  writer.join();
  reader.join();

  EXPECT_EQ(counter, 50);
}

TEST(CircularBuffer, OverrunJumpsIterator) {
  circular_buffer<int> foo{5};
  foo.push_back(1);
  foo.push_back(2);
  auto itr = foo.begin();
  auto const_itr = foo.cbegin();
  EXPECT_EQ(*itr, 1);
  foo.push_back(3);
  foo.push_back(4);
  ++itr;
  ++const_itr;
  EXPECT_EQ(*itr, 2);
  EXPECT_EQ(*const_itr, 2);
  foo.push_back(5);
  foo.push_back(6); // 1 overwritten
  ++itr;
  ++const_itr;
  EXPECT_EQ(*itr, 3);
  EXPECT_EQ(*const_itr, 3);
  foo.push_back(7); // 2 overwritten
  foo.push_back(8); // 3 overwritten
  ++itr;            // Was overrun, but increment back ahead.
  ++const_itr;
  EXPECT_EQ(*itr, 4);
  EXPECT_EQ(*const_itr, 4);
  foo.push_back(9);  // 4 overwritten
  foo.push_back(10); // 5 overwritten
  ++itr;             // Was overrun, increment still behind.
  ++const_itr;
  EXPECT_EQ(*itr, 6); // Jumps forward 2 spaces to new front.
  EXPECT_EQ(*itr, foo.front());
  EXPECT_EQ(*const_itr, 6);
  EXPECT_EQ(*const_itr, foo.front());
}

} // namespace
} // namespace howling
