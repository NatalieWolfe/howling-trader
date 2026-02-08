#pragma once

#include <string>

namespace howling {

struct metrics {
  std::string name;
  double initial_funds = 0;
  double available_funds = 0;
  double assets_value = 0;
  int sales = 0;
  int profitable_sales = 0;
};

void add_metrics(metrics& lhs, const metrics& rhs);

} // namespace howling
