#include "containers/resource_pool.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {
namespace {

using namespace std::chrono_literals;

class mock_resource : public poolable_resource {
public:
  explicit mock_resource(int id = 0) : _id{id} {}

  [[nodiscard]] int id() const { return _id; }
  [[nodiscard]] bool is_healthy() const override { return _healthy; }
  void set_healthy(bool healthy) { _healthy = healthy; }

  void reset() override { ++_reset_count; }

  [[nodiscard]] int reset_count() const { return _reset_count; }

private:
  int _id = 0;
  bool _healthy = true;
  int _reset_count = 0;
};

TEST(ResourcePoolTest, BasicAcquireAndReturn) {
  std::atomic<int> id_counter = 0;
  resource_pool<mock_resource> pool{
      [&]() { return std::make_unique<mock_resource>(++id_counter); },
      {.min_size = 0, .max_size = 2}};

  EXPECT_EQ(pool.size(), 0u);
  EXPECT_EQ(pool.available(), 0u);
  EXPECT_EQ(pool.in_use(), 0u);

  {
    resource_pool<mock_resource>::scoped_resource resource = pool.acquire();
    ASSERT_TRUE(resource);
    EXPECT_EQ(resource->id(), 1);
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.available(), 0u);
    EXPECT_EQ(pool.in_use(), 1u);
  }

  // After resource goes out of scope, it should be back in the pool
  EXPECT_EQ(pool.size(), 1u);
  EXPECT_EQ(pool.available(), 1u);
  EXPECT_EQ(pool.in_use(), 0u);

  // Re-acquire should reuse the same resource
  {
    resource_pool<mock_resource>::scoped_resource resource = pool.acquire();
    ASSERT_TRUE(resource);
    EXPECT_EQ(resource->id(), 1);
    EXPECT_EQ(resource->reset_count(), 1);
    EXPECT_EQ(pool.available(), 0u);
    EXPECT_EQ(pool.in_use(), 1u);
  }
}

TEST(ResourcePoolTest, PrepopulationWithMinSize) {
  std::atomic<int> id_counter = 0;
  resource_pool<mock_resource> pool{
      [&]() { return std::make_unique<mock_resource>(++id_counter); },
      {.min_size = 3, .max_size = 5}};

  EXPECT_EQ(pool.size(), 3u);
  EXPECT_EQ(pool.available(), 3u);
  EXPECT_EQ(pool.in_use(), 0u);
}

TEST(ResourcePoolTest, DiscardsUnhealthyResources) {
  std::atomic<int> id_counter = 0;
  resource_pool<mock_resource> pool{
      [&]() { return std::make_unique<mock_resource>(++id_counter); },
      {.min_size = 1, .max_size = 2}};

  EXPECT_EQ(pool.size(), 1u);

  {
    resource_pool<mock_resource>::scoped_resource resource = pool.acquire();
    EXPECT_EQ(resource->id(), 1);
    resource->set_healthy(false); // Mark unhealthy
  }

  // Pool should discard it on release
  EXPECT_EQ(pool.size(), 0u);
  EXPECT_EQ(pool.available(), 0u);

  // Next acquire creates a new healthy one
  {
    resource_pool<mock_resource>::scoped_resource resource = pool.acquire();
    EXPECT_EQ(resource->id(), 2);
    EXPECT_EQ(pool.size(), 1u);
  }
}

TEST(ResourcePoolTest, BlocksAndAcquiresWhenFull) {
  std::atomic<int> id_counter = 0;
  resource_pool<mock_resource> pool{
      [&]() { return std::make_unique<mock_resource>(++id_counter); },
      {.min_size = 0, .max_size = 1, .acquire_timeout = 2s}};

  resource_pool<mock_resource>::scoped_resource resource1 = pool.acquire();
  EXPECT_EQ(pool.in_use(), 1u);

  std::atomic<bool> acquired_by_thread = false;
  std::jthread worker([&]() {
    resource_pool<mock_resource>::scoped_resource resource2 = pool.acquire();
    if (resource2) { acquired_by_thread = true; }
  });

  std::this_thread::sleep_for(50ms);
  EXPECT_FALSE(acquired_by_thread);

  // Release resource1 to allow worker to acquire
  resource1.reset();
  worker.join();

  EXPECT_TRUE(acquired_by_thread);
}

TEST(ResourcePoolTest, ThrowsOnAcquireTimeout) {
  resource_pool<mock_resource> pool{
      []() { return std::make_unique<mock_resource>(); },
      {.min_size = 0, .max_size = 1, .acquire_timeout = 50ms}};

  resource_pool<mock_resource>::scoped_resource resource1 = pool.acquire();
  EXPECT_THROW((void)pool.acquire(), std::runtime_error);
}

TEST(ResourcePoolTest, ConcurrentAccessStressTest) {
  constexpr int NUM_THREADS = 8;
  constexpr int ITERATIONS = 100;
  std::atomic<int> id_counter = 0;

  resource_pool<mock_resource> pool{
      [&]() { return std::make_unique<mock_resource>(++id_counter); },
      {.min_size = 2, .max_size = 4, .acquire_timeout = 5s}};

  std::vector<std::jthread> threads;
  threads.reserve(NUM_THREADS);

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([&pool]() {
      for (int j = 0; j < ITERATIONS; ++j) {
        resource_pool<mock_resource>::scoped_resource resource = pool.acquire();
        ASSERT_TRUE(resource);
        std::this_thread::sleep_for(100us);
      }
    });
  }

  for (std::jthread& thread : threads) { thread.join(); }

  EXPECT_LE(pool.size(), 4u);
  EXPECT_EQ(pool.in_use(), 0u);
  EXPECT_EQ(pool.available(), pool.size());
}

} // namespace
} // namespace howling
