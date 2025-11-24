#pragma once

#include <chrono>
#include <unordered_map>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/stock.pb.h"

namespace howling {

struct trading_state {
  struct position {
    stock::Symbol symbol;
    double price;
    int quantity;

    double cost() const { return price * quantity; }
  };
  std::unordered_map<stock::Symbol, vector<position>> positions;
  std::unordered_map<stock::Symbol, aggregations> market;
  vector<stock::Symbol> available_stocks;

  double initial_funds;
  double available_funds;
  double total_positions_cost() const;

  std::chrono::system_clock::time_point time_now;
  int market_hour() const;
  int market_minute() const;
  int market_second() const;
  bool market_is_open() const;
};

} // namespace howling
