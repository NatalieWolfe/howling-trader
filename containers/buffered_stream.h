#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <generator>
#include <list>
#include <memory>
#include <mutex>
#include <ranges>
#include <utility>

#include "containers/circular_buffer.h"

namespace howling {

template <typename T>
class buffered_stream {
public:
  explicit buffered_stream(uint32_t buffer_size) : _buffer{buffer_size} {}

  ~buffered_stream() {
    _running = false;
    while (!_readers.empty()) {
      std::lock_guard lock{_readers_mutex};
      for (const auto& reader : _readers) reader->signal.notify_one();
    }
  }

  std::generator<const T&> stream();

  template <typename U>
  void push_back(U&& val) {
    _buffer.push_back(std::forward<U>(val));
    std::lock_guard lock{_readers_mutex};
    for (const auto& reader : _readers) reader->signal.notify_one();
  }

private:
  struct reader_info {
    std::mutex mutex;
    std::condition_variable signal;
  };

  std::atomic_bool _running = true;
  std::mutex _readers_mutex;
  std::list<std::shared_ptr<reader_info>> _readers;
  circular_buffer<T> _buffer;
};

template <typename T>
std::generator<const T&> buffered_stream<T>::stream() {
  auto info = std::make_shared<reader_info>();
  typename std::list<std::shared_ptr<reader_info>>::iterator reader_itr;
  {
    std::lock_guard lock{_readers_mutex};
    _readers.push_front(info);
    reader_itr = _readers.begin();
  }
  auto cleanup_callback = [&reader_itr, this](int* x) {
    delete x;
    std::lock_guard lock{_readers_mutex};
    _readers.erase(reader_itr);
  };
  std::unique_ptr<int, decltype(cleanup_callback)> clean_on_destroy(
      new int, std::move(cleanup_callback));

  auto itr = _buffer.begin();
  while (_running) {
    while (itr != _buffer.end()) {
      co_yield *itr;
      ++itr;
    }
    std::unique_lock lock{info->mutex};
    info->signal.wait(
        lock, [&]() { return itr != _buffer.end() || !_running; });
  }

  clean_on_destroy.reset(nullptr);
}

} // namespace howling
