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
  for (int elem : foo) {
    ASSERT_EQ(elem, ++count);
  }
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
  for (int elem : foo) {
    ASSERT_EQ(elem, ++count + 3);
  }
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
  for (int elem : foo) {
    ASSERT_EQ(elem, ++count + 2);
  }
  EXPECT_EQ(count, 3);
}

} // namespace
} // namespace howling
