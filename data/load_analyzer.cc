#include "data/load_analyzer.h"

#include <format>
#include <memory>
#include <string_view>
#include <utility>

#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/analyzer.pb.h"
#include "data/analyzer_utils.h"
#include "data/analyzers/atr.h"
#include "data/analyzers/bollinger.h"
#include "data/analyzers/ema_ribbon.h"
#include "data/analyzers/howling.h"
#include "data/analyzers/macd.h"
#include "data/analyzers/market_hours.h"
#include "data/analyzers/noop.h"
#include "data/analyzers/pivot_points.h"
#include "data/analyzers/profit.h"
#include "data/analyzers/zig_zag.h"
#include "data/stock.pb.h"

namespace howling {

std::unique_ptr<analyzer>
load_analyzer(std::string_view name, const stock::History& history) {
  if (name.empty() || name == "noop") {
    return std::make_unique<noop_analyzer>();
  }
  if (name == "atr" || name == "atr1") {
    return std::make_unique<atr_analyzer>(&aggregations::one_minute);
  }
  if (name == "atr5") {
    return std::make_unique<atr_analyzer>(&aggregations::five_minute);
  }
  if (name == "atr20") {
    return std::make_unique<atr_analyzer>(&aggregations::twenty_minute);
  }
  if (name == "bollinger") return std::make_unique<bollinger_analyzer>();
  if (name == "ema_ribbon" || name == "ema_ribbon1") {
    return std::make_unique<ema_ribbon_analyzer>(&aggregations::one_minute);
  }
  if (name == "ema_ribbon5") {
    return std::make_unique<ema_ribbon_analyzer>(&aggregations::five_minute);
  }
  if (name == "ema_ribbon20") {
    return std::make_unique<ema_ribbon_analyzer>(&aggregations::twenty_minute);
  }
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
  if (name == "pivot_points") {
    return std::make_unique<pivot_points_analyzer>();
  }
  if (name == "profit") return std::make_unique<profit_analyzer>();
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

std::unique_ptr<analyzer> load_analyzer(const Analyzer& opts) {
  switch (opts.type_case()) {
    case Analyzer::NOOP: {
      return std::make_unique<noop_analyzer>();
    }
    case Analyzer::ATR: {
      atr_analyzer::options atr_options;
      if (opts.atr().period() != 0) {
        atr_options.period = opts.atr().period();
      }
      if (opts.atr().baseline_period() != 0) {
        atr_options.baseline_period = opts.atr().baseline_period();
      }
      if (opts.atr().high_volatility_ratio() != 0.0) {
        atr_options.high_volatility_ratio = opts.atr().high_volatility_ratio();
      }
      return std::make_unique<atr_analyzer>(
          period_member(opts.atr().candle_period()), atr_options);
    }
    case Analyzer::BOLLINGER: {
      return std::make_unique<bollinger_analyzer>();
    }
    case Analyzer::EMA_RIBBON: {
      const Analyzer::EmaRibbon& config = opts.ema_ribbon();
      ema_ribbon_analyzer::options ribbon_options;
      if (!config.periods().empty()) {
        ribbon_options.periods.assign(
            config.periods().begin(), config.periods().end());
      }
      if (config.min_spread_ratio() != 0.0) {
        ribbon_options.min_spread_ratio = config.min_spread_ratio();
      }
      if (config.confidence_scaler() != 0.0) {
        ribbon_options.confidence_scaler = config.confidence_scaler();
      }
      return std::make_unique<ema_ribbon_analyzer>(
          period_member(config.candle_period()), std::move(ribbon_options));
    }
    case Analyzer::HOWLING: {
      return std::make_unique<howling_analyzer>();
    }
    case Analyzer::MACD: {
      // TODO: Support crossover_scaler from analyzer options.
      switch (opts.macd().period()) {
        case Analyzer::Macd::ONE:
          return std::make_unique<macd_crossover_analyzer>(
              &aggregations::one_minute);
        case Analyzer::Macd::FIVE:
          return std::make_unique<macd_crossover_analyzer>(
              &aggregations::five_minute);
        case Analyzer::Macd::TWENTY:
          return std::make_unique<macd_crossover_analyzer>(
              &aggregations::twenty_minute);
        case Analyzer::Macd::PERIOD_UNSPECIFIED:
          throw std::runtime_error("macd.period not specified");
        default:
          throw std::runtime_error(
              std::format(
                  "Unknown MACD period: {} ({}).",
                  Analyzer::Macd::Period_Name(opts.macd().period()),
                  static_cast<int>(opts.macd().period())));
      }
    }
    case Analyzer::MARKET_HOURS: {
      // TODO: Support open and closing hours from proto.
      return std::make_unique<market_hours_analyzer>();
    }
    case Analyzer::PIVOT_POINTS: {
      pivot_points_analyzer::options pivot_options;
      if (opts.pivot_points().proximity_threshold() != 0.0) {
        pivot_options.proximity_threshold =
            opts.pivot_points().proximity_threshold();
      }
      if (opts.pivot_points().confidence_scaler() != 0.0) {
        pivot_options.confidence_scaler =
            opts.pivot_points().confidence_scaler();
      }
      if (opts.pivot_points().major_confidence() != 0.0) {
        pivot_options.major_confidence = opts.pivot_points().major_confidence();
      }
      if (opts.pivot_points().minor_confidence() != 0.0) {
        pivot_options.minor_confidence = opts.pivot_points().minor_confidence();
      }
      return std::make_unique<pivot_points_analyzer>(pivot_options);
    }
    case Analyzer::PROFIT: {
      return std::make_unique<profit_analyzer>();
    }
    case Analyzer::ZIG_ZAG: {
      return std::make_unique<zig_zag_analyzer>(
          stock::History::default_instance(),
          zig_zag_analyzer::options{.threshold = opts.zig_zag().threshold()});
    }
    // case Analyzer::TYPE_UNSPECIFIED: // Same value as TYPE_NOT_SET.
    case Analyzer::TYPE_NOT_SET: {
      throw std::runtime_error("type not specified");
    }
    default: {
      throw std::runtime_error(
          std::format(
              "Unknown analyzer: {} ({}).",
              Analyzer::Type_Name(
                  static_cast<Analyzer::Type>(opts.type_case())),
              static_cast<int>(opts.type_case())));
    }
  }
  throw std::runtime_error("Unreachable code reached in load_analyzer.");
}

} // namespace howling
