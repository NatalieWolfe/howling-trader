#pragma once

#include <limits>
#include <string>

namespace howling {

struct metrics {
  std::string name;
  double initial_funds;
  double available_funds = initial_funds;
  double assets_value = 0;
  int sales = 0;
  int profitable_sales = 0;

  double last_buy_price = -1.0;
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::min();
};

void add_metrics(metrics& lhs, const metrics& rhs);

} // namespace howling
