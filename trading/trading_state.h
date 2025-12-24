#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/stock.pb.h"

namespace howling {

/**
 * Market and account state.
 */
struct trading_state {
  struct position {
    stock::Symbol symbol;
    double price;
    int64_t quantity;

    double cost() const { return price * quantity; }
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
  // Total value of all held positions.
  double total_positions_cost() const;
  double total_positions_value() const;

  // UTC time as of most recent data added.
  std::chrono::system_clock::time_point time_now{};
  // Hour of day in the market timezone.
  int market_hour() const;
  // Minute of current hour in the market timezone.
  int market_minute() const;
  // Second of current minute in the market timezone.
  int market_second() const;
  // Returns true if the market is currently open for trading.
  bool market_is_open() const;
};

} // namespace howling
