#include "data/analyzers/profit.h"

#include <algorithm>

#include "absl/flags/flag.h"
#include "data/analyzer.h"
#include "data/stock.pb.h"
#include "trading/pricing.h"
#include "trading/trading_state.h"

ABSL_FLAG(double, profit_minimum, 0.1, "Minimum profit potential to look for.");
ABSL_FLAG(double, profit_confidence_scaler, 1.0, "");

namespace howling {

decision
profit_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  double current = sale_price(symbol, data);
  double profit_minimum = absl::GetFlag(FLAGS_profit_minimum);
  auto itr = data.positions.find(symbol);
  if (itr == data.positions.end()) return NO_ACTION;

  const trading_state::position* lowest_position = nullptr;
  for (const trading_state::position& position : itr->second) {
    if (!lowest_position || position.price < lowest_position->price) {
      lowest_position = &position;
    }
  }
  if (!lowest_position || lowest_position->price + profit_minimum > current) {
    return NO_ACTION;
  }
  double profit = current - lowest_position->price;
  return {
      .act = action::SELL,
      .confidence = std::min(
          1.0,
          (((profit - profit_minimum) / profit_minimum) + 0.01) *
              absl::GetFlag(FLAGS_profit_confidence_scaler))};
}

} // namespace howling
