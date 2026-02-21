#include "containers/buffered_stream.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "gtest/gtest.h"

namespace howling {
namespace {

using namespace std::chrono_literals;

TEST(BufferedStream, PushAndRetrieve) {
  int counter = 0;
  auto buffer = std::make_unique<buffered_stream<int>>(10000);
  std::thread reader([&]() {
    for (int i : buffer->stream()) EXPECT_EQ(i, ++counter);
  });
  while (buffer->reader_count() < 1) std::this_thread::yield();

  std::thread writer([&]() {
    for (int i = 1; i < 9001; ++i) buffer->push_back(i);
  });
  writer.join();
  buffer = nullptr;
  reader.join();

  EXPECT_EQ(counter, 9000);
}

TEST(BufferedStream, MultipleReaders) {
  auto buffer = std::make_unique<buffered_stream<int>>(10000);
  std::thread writer([&]() {
    for (int i = 1; i < 9001; ++i) buffer->push_back(i);
  });

  std::atomic_int counter = 0;
  auto reader = [&]() {
    int local_counter = 0;
    for (int i : buffer->stream()) {
      EXPECT_EQ(i, ++local_counter);
      ++counter;
    }
    EXPECT_EQ(local_counter, 9000);
  };

  std::thread reader_1(reader);
  std::thread reader_2(reader);
  std::thread reader_3(reader);
  std::thread reader_4(reader);
  while (buffer->reader_count() < 4) std::this_thread::yield();

  writer.join();
  buffer = nullptr;
  reader_1.join();
  reader_2.join();
  reader_3.join();
  reader_4.join();

  EXPECT_EQ(counter, 9000 * 4);
}

TEST(BufferedStream, DestructsCleanly) {
  auto buffer = std::make_unique<buffered_stream<int>>(10);
  buffer = nullptr;
  SUCCEED();
}

TEST(BufferedStream, EmptyReadTerminatesCleanly) {
  auto buffer = std::make_unique<buffered_stream<int>>(10);
  std::atomic_bool reader_finished = false;

  std::thread reader([&]() {
    for (int i : buffer->stream()) {
      FAIL() << "Reader received unexpected data: " << i;
    }
    reader_finished = true;
  });

  while (buffer->reader_count() < 1) std::this_thread::yield();

  // Give the reader time to start and wait.
  std::this_thread::sleep_for(20ms);
  buffer = nullptr;
  reader.join();

  EXPECT_TRUE(reader_finished);
}

TEST(BufferedStream, ReadersCatchUpOnBufferedData) {
  auto buffer = std::make_unique<buffered_stream<int>>(100);
  for (int i = 1; i <= 100; ++i) buffer->push_back(i);

  std::atomic_int counter = 0;
  std::thread reader([&]() {
    for (int i : buffer->stream()) EXPECT_EQ(i, ++counter);
  });

  while (buffer->reader_count() < 1) std::this_thread::yield();

  // Give the late reader time to process the existing items.
  std::this_thread::sleep_for(20ms);
  buffer = nullptr;
  reader.join();

  EXPECT_EQ(counter, 100);
}

TEST(BufferedStream, SlowReaderLosesData) {
  auto buffer = std::make_unique<buffered_stream<int>>(10);
  std::thread writer([&]() {
    while (buffer->reader_count() < 1) std::this_thread::sleep_for(1ms);

    for (int i = 1; i <= 100; ++i) {
      buffer->push_back(i);
      std::this_thread::sleep_for(250us);
    }
  });

  std::atomic_int counter = 0;
  std::atomic_int last_seen = 0;
  std::thread reader([&]() {
    for (int i : buffer->stream()) {
      EXPECT_GT(i, last_seen); // Data should still be in increasing order.
      last_seen = i;
      ++counter;
      std::this_thread::sleep_for(1ms);
    }
  });

  while (buffer->reader_count() < 1) std::this_thread::yield();

  writer.join();
  // Wait for the reader to finish processing items up to 100 or exit.
  buffer = nullptr;
  reader.join();

  // The slow reader won't see all items (count <100), but it will see the final
  // `buffer.size()` items.
  EXPECT_EQ(last_seen, 100);
  EXPECT_GE(counter, 10);
  EXPECT_LT(counter, 100);
}

TEST(BufferedStream, ReaderDisconnect) {
  auto buffer = std::make_unique<buffered_stream<int>>(100);
  std::atomic_bool writer_done = false;
  std::atomic_bool reader_done = false;
  std::thread writer([&]() {
    while (buffer->reader_count() < 1) std::this_thread::sleep_for(1ms);

    for (int i = 1; i < 1000; ++i) {
      buffer->push_back(i);
      std::this_thread::sleep_for(1ms);
    }
    writer_done = true;
  });

  std::thread reader([&]() {
    for (int i : buffer->stream()) {
      std::this_thread::sleep_for(1ms);
      if (i > 100) break;
    }
    reader_done = true;
  });

  while (buffer->reader_count() < 1) std::this_thread::yield();

  EXPECT_FALSE(writer_done);
  EXPECT_FALSE(reader_done);
  reader.join();
  EXPECT_FALSE(writer_done);
  EXPECT_TRUE(reader_done);
  writer.join();
  EXPECT_TRUE(writer_done);
  EXPECT_TRUE(reader_done);
}

} // namespace
} // namespace howling
