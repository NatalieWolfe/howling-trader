#include "containers/resource_pool.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace howling {
namespace resource_pool_internal {

base_resource_pool::base_resource_pool(
    resource_factory factory, options pool_options)
    : _factory{std::move(factory)}, _options{pool_options} {
  if (!_factory) {
    throw std::invalid_argument("Resource pool factory cannot be null.");
  }
  if (_options.max_size == 0) {
    throw std::invalid_argument(
        "Resource pool max_size must be greater than 0.");
  }
  if (_options.min_size > _options.max_size) {
    throw std::invalid_argument(
        "Resource pool min_size cannot exceed max_size.");
  }

  // Pre-populate min_size resources
  for (size_t i = 0; i < _options.min_size; ++i) {
    std::unique_ptr<poolable_resource> resource = _factory();
    if (resource && resource->is_healthy()) {
      _available.push_back(
          {std::move(resource), std::chrono::steady_clock::now()});
      ++_total_allocated;
    }
  }
}

base_resource_pool::~base_resource_pool() {
  close();
}

std::unique_ptr<poolable_resource> base_resource_pool::acquire() {
  std::unique_lock<std::mutex> lock{_mutex};

  std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + _options.acquire_timeout;

  while (true) {
    if (_closed) throw std::runtime_error("Resource pool is closed.");

    _prune_idle_locked();

    // Check if an available idle resource can be reused
    while (!_available.empty()) {
      idle_entry entry = std::move(_available.back());
      _available.pop_back();

      if (entry.resource && entry.resource->is_healthy()) {
        return std::move(entry.resource);
      }

      // Unhealthy resource: discard
      entry.resource.reset();
      _decrement_total_allocated();
    }

    // No available resource, can we allocate a new one?
    if (_total_allocated < _options.max_size) {
      ++_total_allocated;
      lock.unlock();

      std::unique_ptr<poolable_resource> new_resource;
      try {
        new_resource = _factory();
      } catch (...) {
        lock.lock();
        _decrement_total_allocated();
        _cv.notify_one();
        throw;
      }

      if (!new_resource || !new_resource->is_healthy()) {
        lock.lock();
        _decrement_total_allocated();
        _cv.notify_one();
        throw std::runtime_error(
            "Resource factory failed to create a healthy resource.");
      }

      return new_resource;
    }

    // At capacity: wait for a resource to be returned
    if (_options.acquire_timeout <= std::chrono::milliseconds::zero()) {
      _cv.wait(lock, [this]() { return _can_acquire_locked(); });
    } else {
      if (_cv.wait_until(lock, deadline, [this]() {
            return _can_acquire_locked();
          }) == false) {
        throw std::runtime_error("Resource pool acquire timed out.");
      }
    }
  }
}

void base_resource_pool::release(std::unique_ptr<poolable_resource> resource) {
  if (!resource) return;

  // Check health and reset outside of lock where possible
  bool healthy = resource->is_healthy();
  if (healthy) {
    try {
      resource->reset();
    } catch (...) { healthy = false; }
  }

  std::unique_ptr<poolable_resource> to_destroy;
  {
    std::lock_guard<std::mutex> lock{_mutex};

    if (_closed || !healthy || _total_allocated > _options.max_size) {
      _decrement_total_allocated();
      to_destroy = std::move(resource);
    } else {
      _available.push_back(
          {std::move(resource), std::chrono::steady_clock::now()});
    }
    _cv.notify_one();
  }
}

size_t base_resource_pool::size() const {
  std::lock_guard<std::mutex> lock{_mutex};
  return _total_allocated;
}

size_t base_resource_pool::available() const {
  std::lock_guard<std::mutex> lock{_mutex};
  return _available.size();
}

size_t base_resource_pool::in_use() const {
  std::lock_guard<std::mutex> lock{_mutex};
  return _total_allocated >= _available.size()
      ? _total_allocated - _available.size()
      : 0;
}

size_t base_resource_pool::capacity() const {
  std::lock_guard<std::mutex> lock{_mutex};
  return _options.max_size;
}

void base_resource_pool::close() {
  std::deque<idle_entry> to_destroy;
  {
    std::lock_guard<std::mutex> lock{_mutex};
    _closed = true;
    to_destroy = std::move(_available);
    _total_allocated = 0;
    _cv.notify_all();
  }
}

void base_resource_pool::_decrement_total_allocated() {
  if (_total_allocated > 0) --_total_allocated;
}

bool base_resource_pool::_can_acquire_locked() const {
  return _closed || !_available.empty() || _total_allocated < _options.max_size;
}

void base_resource_pool::_prune_idle_locked() {
  if (_options.idle_timeout <= std::chrono::milliseconds::zero()) return;

  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  while (_available.size() > _options.min_size) {
    idle_entry& front = _available.front();
    if (now - front.last_used < _options.idle_timeout) break;

    _available.pop_front();
    _decrement_total_allocated();
  }
}

} // namespace resource_pool_internal
} // namespace howling
