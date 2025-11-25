#include "data/load_analyzer.h"

#include <memory>
#include <string_view>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzers/bollinger.h"
#include "data/analyzers/howling.h"
#include "data/analyzers/macd.h"
#include "data/analyzers/market_hours.h"
#include "data/analyzers/noop.h"
#include "data/analyzers/zig_zag.h"
#include "data/stock.pb.h"

namespace howling {

std::unique_ptr<analyzer>
load_analyzer(std::string_view name, const stock::History& history) {
  if (name.empty() || name == "noop") {
    return std::make_unique<noop_analyzer>();
  }
  if (name == "bollinger") return std::make_unique<bollinger_analyzer>();
  if (name == "howling") return std::make_unique<howling_analyzer>();
  if (name == "macd" || name == "macd1") {
    return std::make_unique<macd_crossover_analyzer>(&aggregations::one_minute);
  }
  if (name == "macd5") {
    return std::make_unique<macd_crossover_analyzer>(
        &aggregations::five_minute);
  }
  if (name == "macd20") {
    return std::make_unique<macd_crossover_analyzer>(
        &aggregations::twenty_minute);
  }
  if (name == "market_hours") {
    return std::make_unique<market_hours_analyzer>();
  }
  if (name == "zig_zag" || name == "optimal") {
    if (history.candles().empty()) {
      throw std::runtime_error(
          "ZigZag analyzer requires fore-knowledge of market movements.");
    }
    return std::make_unique<zig_zag_analyzer>(
        history, zig_zag_analyzer::options{.threshold = 0.5});
  }
  throw std::runtime_error(absl::StrCat("Unknown analyzer: ", name));
}

std::unique_ptr<analyzer> load_analyzer(std::string_view name) {
  return load_analyzer(name, stock::History::default_instance());
}

} // namespace howling
