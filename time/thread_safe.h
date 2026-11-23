#pragma once

#include <atomic>
#include <chrono>
#include <compare>

namespace howling {

/**
 * @brief Thread-safe wrapper around std::chrono::system_clock::time_point.
 *
 * This class is thread-safe (internally synchronized) using a lock-free
 * std::atomic<std::chrono::system_clock::rep>. Individual operations (load,
 * store, exchange, implicit conversion, and assignment from a time_point) are
 * atomic and lock-free.
 *
 * Unlike std::atomic directly, atomic_time_point is copyable and movable so
 * that enclosing structs remain aggregate-initializable (including with
 * designated initializers), copyable, and movable. Copy operations atomically
 * load from the source and initialize or store into the destination as separate
 * atomic operations; move operations are intentionally omitted so rvalues fall
 * back to copy.
 */
class atomic_time_point {
public:
  /**
   * @brief Constructs an atomic_time_point initialized to the clock epoch.
   */
  constexpr atomic_time_point() noexcept = default;

  /**
   * @brief Constructs an atomic_time_point from the given time point.
   */
  constexpr atomic_time_point(
      std::chrono::system_clock::time_point value) noexcept
      : _ticks(value.time_since_epoch().count()) {}

  /**
   * @brief Copy-constructs by atomically loading the value from other.
   */
  atomic_time_point(const atomic_time_point& other) noexcept
      : _ticks(other._ticks.load(std::memory_order_seq_cst)) {}

  /**
   * @brief Copy-assigns by atomically loading from other and storing into this
   * instance.
   */
  atomic_time_point& operator=(const atomic_time_point& other) noexcept {
    if (this != &other) store(other.load());
    return *this;
  }

  /**
   * @brief Atomically stores the given time point into this instance.
   */
  atomic_time_point&
  operator=(std::chrono::system_clock::time_point value) noexcept {
    store(value);
    return *this;
  }

  /**
   * @brief Atomically loads and returns the stored time point.
   */
  [[nodiscard]] std::chrono::system_clock::time_point
  load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return _from_ticks(_ticks.load(order));
  }

  /**
   * @brief Atomically stores the given time point.
   */
  void store(
      std::chrono::system_clock::time_point value,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    _ticks.store(value.time_since_epoch().count(), order);
  }

  /**
   * @brief Atomically replaces the stored time point and returns the previous
   * value.
   */
  std::chrono::system_clock::time_point exchange(
      std::chrono::system_clock::time_point value,
      std::memory_order order = std::memory_order_seq_cst) noexcept {
    return _from_ticks(
        _ticks.exchange(value.time_since_epoch().count(), order));
  }

  /**
   * @brief Implicitly converts to std::chrono::system_clock::time_point via an
   * atomic load.
   */
  [[nodiscard]]
  operator std::chrono::system_clock::time_point() const noexcept {
    return load();
  }

  /**
   * @brief Compares two atomic_time_point instances for equality.
   */
  [[nodiscard]] friend bool operator==(
      const atomic_time_point& left, const atomic_time_point& right) noexcept {
    return left.load() == right.load();
  }

  /**
   * @brief Compares an atomic_time_point and a time_point for equality.
   */
  [[nodiscard]] friend bool operator==(
      const atomic_time_point& left,
      std::chrono::system_clock::time_point right) noexcept {
    return left.load() == right;
  }

  /**
   * @brief Three-way compares two atomic_time_point instances.
   */
  [[nodiscard]] friend std::strong_ordering operator<=>(
      const atomic_time_point& left, const atomic_time_point& right) noexcept {
    return left.load() <=> right.load();
  }

  /**
   * @brief Three-way compares an atomic_time_point and a time_point.
   */
  [[nodiscard]] friend std::strong_ordering operator<=>(
      const atomic_time_point& left,
      std::chrono::system_clock::time_point right) noexcept {
    return left.load() <=> right;
  }

  /**
   * @brief Returns the duration between two atomic_time_point instances.
   */
  [[nodiscard]] friend std::chrono::system_clock::duration operator-(
      const atomic_time_point& left, const atomic_time_point& right) noexcept {
    return left.load() - right.load();
  }

  /**
   * @brief Returns the duration between an atomic_time_point and a time_point.
   */
  [[nodiscard]] friend std::chrono::system_clock::duration operator-(
      const atomic_time_point& left,
      std::chrono::system_clock::time_point right) noexcept {
    return left.load() - right;
  }

  /**
   * @brief Returns the duration between a time_point and an atomic_time_point.
   */
  [[nodiscard]] friend std::chrono::system_clock::duration operator-(
      std::chrono::system_clock::time_point left,
      const atomic_time_point& right) noexcept {
    return left - right.load();
  }

  /**
   * @brief Adds a duration to an atomic_time_point snapshot.
   */
  template <typename Rep, typename Period>
  [[nodiscard]] friend auto operator+(
      const atomic_time_point& left,
      const std::chrono::duration<Rep, Period>& right) noexcept {
    return left.load() + right;
  }

  /**
   * @brief Adds an atomic_time_point snapshot to a duration.
   */
  template <typename Rep, typename Period>
  [[nodiscard]] friend auto operator+(
      const std::chrono::duration<Rep, Period>& left,
      const atomic_time_point& right) noexcept {
    return left + right.load();
  }

  /**
   * @brief Subtracts a duration from an atomic_time_point snapshot.
   */
  template <typename Rep, typename Period>
  [[nodiscard]] friend auto operator-(
      const atomic_time_point& left,
      const std::chrono::duration<Rep, Period>& right) noexcept {
    return left.load() - right;
  }

private:
  static_assert(
      std::atomic<std::chrono::system_clock::rep>::is_always_lock_free,
      "atomic_time_point requires a lock-free system_clock::rep.");

  static constexpr std::chrono::system_clock::time_point
  _from_ticks(std::chrono::system_clock::rep ticks) noexcept {
    return std::chrono::system_clock::time_point{
        std::chrono::system_clock::duration{ticks}};
  }

  // Stores the underlying tick count from time_since_epoch().count().
  std::atomic<std::chrono::system_clock::rep> _ticks{0};
};

} // namespace howling
