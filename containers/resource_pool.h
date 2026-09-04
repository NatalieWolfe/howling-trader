#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace howling {

/**
 * @brief Mixin interface for any resource managed by a resource_pool.
 */
class poolable_resource {
public:
  virtual ~poolable_resource() = default;

  /**
   * @brief Returns true if the resource is healthy and valid for use.
   *
   * If false, the pool will discard the resource and create a new one.
   */
  [[nodiscard]] virtual bool is_healthy() const = 0;

  /**
   * @brief Resets any state before the resource is returned or reused.
   */
  virtual void reset() {}
};

namespace resource_pool_internal {

struct options {
  size_t min_size = 0;
  size_t max_size = 10;
  std::chrono::milliseconds acquire_timeout = std::chrono::seconds(30);
  std::chrono::milliseconds idle_timeout = std::chrono::minutes(5);
};

/**
 * @brief Non-templated base class that manages synchronization, allocation,
 * health checks, and lifecycle of poolable_resource instances.
 */
class base_resource_pool {
public:
  using resource_factory = std::function<std::unique_ptr<poolable_resource>()>;

  explicit base_resource_pool(
      resource_factory factory, options pool_options = {});
  virtual ~base_resource_pool();

  base_resource_pool(const base_resource_pool&) = delete;
  base_resource_pool& operator=(const base_resource_pool&) = delete;
  base_resource_pool(base_resource_pool&&) = delete;
  base_resource_pool& operator=(base_resource_pool&&) = delete;

  /**
   * @brief Acquires a healthy resource from the pool, waiting if necessary.
   */
  std::unique_ptr<poolable_resource> acquire();

  /**
   * @brief Returns a resource back to the pool.
   */
  void release(std::unique_ptr<poolable_resource> resource);

  /**
   * @brief Total number of resources currently managed (both in use and idle).
   */
  [[nodiscard]] size_t size() const;

  /**
   * @brief Number of idle resources available in the pool.
   */
  [[nodiscard]] size_t available() const;

  /**
   * @brief Number of resources currently checked out and in use.
   */
  [[nodiscard]] size_t in_use() const;

  /**
   * @brief Maximum capacity of the pool.
   */
  [[nodiscard]] size_t capacity() const;

  /**
   * @brief Closes the pool, destroying idle resources and notifying waiters.
   */
  void close();

private:
  struct idle_entry {
    std::unique_ptr<poolable_resource> resource;
    std::chrono::steady_clock::time_point last_used;
  };

  void _prune_idle_locked();
  void _decrement_total_allocated();
  [[nodiscard]] bool _can_acquire_locked() const;

  resource_factory _factory;
  options _options;
  mutable std::mutex _mutex;
  std::condition_variable _cv;
  std::deque<idle_entry> _available;
  size_t _total_allocated = 0;
  bool _closed = false;
};

} // namespace resource_pool_internal

/**
 * @brief Thread-safe template resource pool returning RAII scoped pointers.
 */
template <typename T>
class resource_pool : public resource_pool_internal::base_resource_pool {
  static_assert(
      std::is_base_of_v<poolable_resource, T>,
      "T must inherit from howling::poolable_resource");

public:
  using resource_factory = std::function<std::unique_ptr<T>()>;
  using options = resource_pool_internal::options;

  /**
   * @brief RAII handle that returns the resource to the pool upon destruction.
   */
  class scoped_resource {
  public:
    scoped_resource() = default;

    scoped_resource(
        resource_pool<T>& pool, std::unique_ptr<poolable_resource> resource)
        : _pool{&pool},
          _resource{std::unique_ptr<T>(static_cast<T*>(resource.release()))} {}

    ~scoped_resource() { reset(); }

    scoped_resource(scoped_resource&& other) noexcept
        : _pool{other._pool}, _resource{std::move(other._resource)} {
      other._pool = nullptr;
    }

    scoped_resource& operator=(scoped_resource&& other) noexcept {
      if (this != &other) {
        reset();
        _pool = other._pool;
        _resource = std::move(other._resource);
        other._pool = nullptr;
      }
      return *this;
    }

    scoped_resource(const scoped_resource&) = delete;
    scoped_resource& operator=(const scoped_resource&) = delete;

    [[nodiscard]] T* get() const noexcept { return _resource.get(); }
    [[nodiscard]] T* operator->() const noexcept { return _resource.get(); }
    [[nodiscard]] T& operator*() const noexcept { return *_resource; }
    [[nodiscard]] explicit operator bool() const noexcept {
      return static_cast<bool>(_resource);
    }

    void reset() {
      if (_pool && _resource) {
        _pool->release(std::unique_ptr<poolable_resource>(_resource.release()));
      }
      _pool = nullptr;
      _resource.reset();
    }

  private:
    resource_pool<T>* _pool = nullptr;
    std::unique_ptr<T> _resource;
  };

  explicit resource_pool(resource_factory factory, options pool_options = {})
      : base_resource_pool(
            [f = std::move(factory)]() -> std::unique_ptr<poolable_resource> {
              return f();
            },
            pool_options) {}

  [[nodiscard]] scoped_resource acquire() {
    std::unique_ptr<poolable_resource> resource =
        base_resource_pool::acquire();
    return scoped_resource{*this, std::move(resource)};
  }
};

} // namespace howling
