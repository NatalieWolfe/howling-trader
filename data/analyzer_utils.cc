#include "data/analyzer_utils.h"

#include <chrono>
#include <format>
#include <stdexcept>

#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.pb.h"
#include "data/stock.pb.h"
#include "trading/trading_state.h"

namespace howling {

const aggregations*
find_aggregations(stock::Symbol symbol, const trading_state& data) {
  auto itr = data.market.find(symbol);
  if (itr == data.market.end()) return nullptr;
  return &itr->second;
}

int window_size_for(const vector<window>& period) {
  return period.empty() ? 0 : period(-1).count;
}

bool is_window_step_boundary(
    const trading_state& data, std::chrono::minutes window_minutes) {
  if (window_minutes <= std::chrono::minutes{0} ||
      window_minutes >= std::chrono::minutes{60}) {
    throw std::invalid_argument(
        "Window minutes must be strictly between 0 and 60.");
  }
  return data.market_minute() % window_minutes.count() == 0;
}

vector<window> aggregations::* period_member(Analyzer::Period period) {
  switch (period) {
    case Analyzer::ONE:
      return &aggregations::one_minute;
    case Analyzer::FIVE:
      return &aggregations::five_minute;
    case Analyzer::TWENTY:
      return &aggregations::twenty_minute;
    case Analyzer::PERIOD_UNSPECIFIED:
      throw std::invalid_argument("Analyzer period must be specified.");
    default:
      throw std::invalid_argument(
          std::format(
              "Unknown analyzer period value: {} ({}).",
              Analyzer::Period_Name(period),
              static_cast<int>(period)));
  }
}

} // namespace howling
