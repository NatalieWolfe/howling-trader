#include "data/analyzers/zig_zag.h"

#include <cstdint>
#include <limits>

#include "containers/vector.h"
#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "data/trading_state.h"

namespace howling {
namespace {

enum class trend { NONE, UP, DOWN };

}

zig_zag_analyzer::zig_zag_analyzer(
    const stock::History& full_history, options opts)
    : _symbol{full_history.symbol()} {
  const auto& candles = full_history.candles();

  trend trend = trend::NONE;
  double last_high = candles.at(0).high();
  double last_low = candles.at(0).low();

  int last_high_idx = 0;
  int last_low_idx = 0;

  for (size_t i = 0; i < candles.size(); ++i) {
    const Candle& candle = candles.at(i);
    if (trend == trend::NONE) {
      // Find initial trend direction.
      if (candle.high() > last_high + opts.threshold) {
        trend = trend::UP;
        _buy_points.push_back(candles.at(last_low_idx));
        last_high = candle.high();
        last_high_idx = i;
      } else if (candle.low() < last_low - opts.threshold) {
        trend = trend::DOWN;
        if (_buy_points.size() > _sell_points.size()) {
          _sell_points.push_back(candles.at(last_high_idx));
        }
        last_low = candle.low();
        last_low_idx = i;
      }
      if (candle.low() < last_low) {
        last_low = candle.low();
        last_low_idx = i;
      } else if (candle.high() > last_high) {
        last_high = candle.high();
        last_high_idx = i;
      }
    } else if (trend == trend::UP) {
      // Watch for the reversal to sell off.
      if (candle.high() > last_high) {
        last_high = candle.high();
        last_high_idx = i;
      } else if (candle.low() < last_high - opts.threshold) {
        trend = trend::DOWN;
        _sell_points.push_back(candles.at(last_high_idx));
        last_low = candle.low();
        last_low_idx = i;
      }
    } else if (trend == trend::DOWN) {
      // Watch for the reversal to buy.
      if (candle.low() < last_low) {
        last_low = candle.low();
        last_low_idx = i;
      } else if (candle.high() > last_low + opts.threshold) {
        trend = trend::UP;
        _buy_points.push_back(candles.at(last_low_idx));
        last_high = candle.high();
        last_high_idx = i;
      }
    }
  }
}

decision
zig_zag_analyzer::analyze(stock::Symbol symbol, const trading_state& data) {
  const Candle& candle = data.market.at(_symbol).one_minute(-1).candle;
  for (const Candle& buy : _buy_points) {
    if (candle.opened_at().seconds() == buy.opened_at().seconds()) {
      return {.act = action::BUY, .confidence = 1.0};
    }
  }
  for (const Candle& sell : _sell_points) {
    if (candle.opened_at().seconds() == sell.opened_at().seconds()) {
      return {.act = action::SELL, .confidence = 1.0};
    }
  }
  return {.act = action::HOLD, .confidence = 1.0};
}

} // namespace howling
