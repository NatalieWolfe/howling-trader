#pragma once

#include <string>
#include "containers/vector.h"

namespace howling {

struct metrics {
  std::string name;
  double initial_funds = 0;
  double available_funds = 0;
  double assets_value = 0;
  int sales = 0;
  int profitable_sales = 0;
  vector<double> deltas;
};

void add_metrics(metrics& lhs, const metrics& rhs);

} // namespace howling
