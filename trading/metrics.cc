#include "trading/metrics.h"

namespace howling {

void add_metrics(metrics& lhs, const metrics& rhs) {
  lhs.available_funds = rhs.available_funds;
  lhs.sales += rhs.sales;
  lhs.profitable_sales += rhs.profitable_sales;
  for (double d : rhs.deltas) lhs.deltas.push_back(d);
}

} // namespace howling
