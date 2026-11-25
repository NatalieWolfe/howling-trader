#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/stock.pb.h"
#include "time/thread_safe.h"

namespace howling {

/**
 * @brief Market and account state for a trading session.
 *
 * Thread-compatible overall: external synchronization is required when
 * concurrently mutating containers (`positions`, `market`, `available_stocks`)
 * or account balances. The `time_now` field (`howling::atomic_time_point`) and
 * the market-clock accessors (`market_hour()`, `market_minute()`,
 * `market_second()`, `market_is_open()`) are internally synchronized and
 * thread-safe for concurrent reads and writes across threads.
 */
struct trading_state {
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
