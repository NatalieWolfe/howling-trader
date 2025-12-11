#include "containers/circular_buffer.h"

#include "gtest/gtest.h"

namespace howling {
namespace {

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

} // namespace
} // namespace howling
