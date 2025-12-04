#include "trading/metrics.h"

#include <algorithm>

namespace howling {

void add_metrics(metrics& lhs, const metrics& rhs) {
  lhs.available_funds = rhs.available_funds;
  lhs.sales += rhs.sales;
  lhs.profitable_sales += rhs.profitable_sales;

  lhs.last_buy_price = rhs.last_buy_price;
  lhs.min = std::min(lhs.min, rhs.min);
  lhs.max = std::max(lhs.max, rhs.max);
}

} // namespace howling
