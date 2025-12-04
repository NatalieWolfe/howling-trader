#pragma once

#include <string>

namespace howling {

struct metrics {
  std::string name;
  double initial_funds;
  double available_funds = initial_funds;
  double assets_value = 0;
  int sales = 0;
  int profitable_sales = 0;
};

void add_metrics(metrics& lhs, const metrics& rhs);

std::string print_metrics(const metrics& m);

} // namespace howling
