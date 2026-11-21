#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/stock.pb.h"

namespace howling {

/**
 * @brief Market and account state for a trading session.
 *
 * Thread-compatible overall: external synchronization is required when
 * concurrently mutating containers (`positions`, `market`, `available_stocks`)
 * or account balances. The `time_now` field and the market-clock accessors
 * (`market_hour()`, `market_minute()`, `market_second()`, `market_is_open()`)
 * are internally synchronized via `std::atomic` and are thread-safe for
 * concurrent reads and writes across threads.
 */
struct trading_state {
  /**
   * @brief Thread-safe wrapper around `std::chrono::system_clock::time_point`.
   *
   * Internally synchronized via `std::atomic` so concurrent updates from the
   * candle stream thread and reads from other threads are race-free. Supports
   * copy and move operations so `trading_state` remains an aggregate and
   * movable.
   */
  class atomic_time_point {
  public:
    static_assert(
        std::is_trivially_copyable_v<std::chrono::system_clock::time_point>,
        "system_clock::time_point must be trivially copyable for std::atomic.");

    atomic_time_point() noexcept = default;
    atomic_time_point(std::chrono::system_clock::time_point value) noexcept
        : _value{value} {}
    atomic_time_point(const atomic_time_point& other) noexcept
        : _value{other.load()} {}
    atomic_time_point(atomic_time_point&& other) noexcept
        : _value{other.load()} {}

    atomic_time_point& operator=(const atomic_time_point& other) noexcept {
      store(other.load());
      return *this;
    }

    atomic_time_point& operator=(atomic_time_point&& other) noexcept {
      store(other.load());
      return *this;
    }

    atomic_time_point&
    operator=(std::chrono::system_clock::time_point value) noexcept {
      store(value);
      return *this;
    }

    /**
     * @brief Atomically loads the stored timestamp.
     */
    [[nodiscard]] std::chrono::system_clock::time_point
    load(std::memory_order order = std::memory_order_acquire) const noexcept {
      return _value.load(order);
    }

    /**
     * @brief Atomically stores a new timestamp.
     */
    void store(
        std::chrono::system_clock::time_point value,
        std::memory_order order = std::memory_order_release) noexcept {
      _value.store(value, order);
    }

    operator std::chrono::system_clock::time_point() const noexcept {
      return load();
    }

  private:
    std::atomic<std::chrono::system_clock::time_point> _value{
        std::chrono::system_clock::time_point{}};
  };

  struct position {
    stock::Symbol symbol;
    double price;
    int64_t quantity;

    /**
     * @brief Returns the total purchase cost of this position.
     */
    [[nodiscard]] double cost() const { return price * quantity; }
  };

  // Bought positions on each stock.
  std::unordered_map<stock::Symbol, vector<position>> positions;
  // Current market conditions for each stock.
  std::unordered_map<stock::Symbol, aggregations> market;
  // Stocks which may be analyzed.
  vector<stock::Symbol> available_stocks;

  std::string account_id;
  // Amount of funds at the beginning of the trading session.
  double initial_funds;
  // Currently liquid funds available for purchasing shares.
  double available_funds;

  /**
   * @brief Returns the total purchase cost of all held positions.
   */
  [[nodiscard]] double total_positions_cost() const;

  /**
   * @brief Returns the total current market value of all held positions.
   */
  [[nodiscard]] double total_positions_value() const;

  // UTC time as of most recent data added. Internally synchronized.
  atomic_time_point time_now{};

  /**
   * @brief Returns the hour of day in the market timezone (thread-safe).
   */
  [[nodiscard]] int market_hour() const;

  /**
   * @brief Returns the minute of the current hour in the market timezone
   * (thread-safe).
   */
  [[nodiscard]] int market_minute() const;

  /**
   * @brief Returns the second of the current minute in the market timezone
   * (thread-safe).
   */
  [[nodiscard]] int market_second() const;

  /**
   * @brief Returns true if the market is currently open for trading
   * (thread-safe).
   */
  [[nodiscard]] bool market_is_open() const;
};

} // namespace howling
