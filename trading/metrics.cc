#include "trading/metrics.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "cli/colorize.h"
#include "cli/printing.h"

namespace howling {

void add_metrics(metrics& lhs, const metrics& rhs) {
  lhs.available_funds = rhs.available_funds;
  lhs.sales += rhs.sales;
  lhs.profitable_sales += rhs.profitable_sales;
}

std::string print_metrics(const metrics& m) {
  double profit = m.available_funds + m.assets_value - m.initial_funds;
  return absl::StrCat(
      m.name,
      "\n  Sales:   ",
      m.sales,
      "\n  +Sales:  ",
      m.profitable_sales,
      "\n  $ Delta: ",
      colorize(
          print_price(profit),
          profit > 0.0       ? color::GREEN
              : profit < 0.0 ? color::RED
                             : color::GRAY));
}

} // namespace howling
