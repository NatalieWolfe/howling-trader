#include "data/aggregate.h"

#include <algorithm>
#include <span>
#include <vector>

#include "containers/vector.h"
#include "data/candle.pb.h"

namespace howling {
namespace {

window blank_window() {
  return {
      .candle = Candle::default_instance(),
      .green_body = false,
      .body_high = 0.0,
      .body_low = 0.0,
      .price_delta = 0.0,
      .upper_wick_length = 0.0,
      .lower_wick_length = 0.0,
      .total_wick_length = 0.0,
      .wick_body_ratio = 0.0,
      .moving_average = 0.0,
      .stddev = 0.0,
      .upper_bollinger_band = 0.0,
      .lower_bollinger_band = 0.0,
      .green_sequence = false,
      .setup_counter = 0,
      .countdown_counter = 0,
  };
}

window to_window(const Candle& candle) {
  double body_high = std::max(candle.open(), candle.close());
  double body_low = std::min(candle.open(), candle.close());
  window w{
      .candle = candle,
      .green_body = candle.close() > candle.open(),
      .body_high = body_high,
      .body_low = body_low,
      .price_delta = body_high - body_low,
      .upper_wick_length = candle.high() - body_high,
      .lower_wick_length = body_low - candle.low(),
      .moving_average = 0,
      .stddev = 0,
      .upper_bollinger_band = 0,
      .lower_bollinger_band = 0,
      .green_sequence = false,
      .setup_counter = 0,
      .countdown_counter = 0,
  };
  w.total_wick_length = w.upper_wick_length + w.lower_wick_length;
  w.wick_body_ratio = w.total_wick_length / w.price_delta;
  return w;
}

void add_next_window(window& a, const window& b) {
  a.candle.set_close(b.candle.close());
  a.candle.set_high(std::max(a.candle.high(), b.candle.high()));
  a.candle.set_low(std::min(a.candle.low(), b.candle.low()));
  a.candle.set_volume(a.candle.volume() + b.candle.volume());
  // TODO: Add b.duration to a.duration

  window combined = to_window(a.candle);
  a.green_body = combined.green_body;
  a.body_high = combined.body_high;
  a.body_low = combined.body_low;
  a.price_delta = combined.price_delta;

  a.upper_wick_length = combined.upper_wick_length;
  a.lower_wick_length = combined.lower_wick_length;
  a.total_wick_length = combined.total_wick_length;
  a.wick_body_ratio = combined.wick_body_ratio;

  a.moving_average += b.candle.close();
}

window do_aggregate(std::span<const Candle> one_minute_candles) {
  window w = blank_window();
  for (const Candle& candle : one_minute_candles) {
    add_next_window(w, to_window(candle));
  }
  w.moving_average /= one_minute_candles.size();
  double sq_diff_sum = 0;
  for (const Candle& candle : one_minute_candles) {
    double diff = candle.close() - w.moving_average;
    sq_diff_sum += diff * diff;
  }
  w.stddev = std::sqrt(sq_diff_sum);
  w.upper_bollinger_band = w.moving_average + (2.0 * w.stddev);
  w.lower_bollinger_band = w.moving_average - (2.0 * w.stddev);

  return w;
}

} // namespace

aggregations aggregate(const vector<Candle>& one_minute_candles) {
  aggregations aggr;

  for (size_t i = 0; i < one_minute_candles.size(); ++i) {
    const Candle& candle = one_minute_candles.at(i);
    aggr.one_minute.push_back(to_window(candle));
    aggr.five_minute.push_back(do_aggregate(one_minute_candles.last_n(5)));
    aggr.twenty_minute.push_back(do_aggregate(one_minute_candles.last_n(20)));

    // TODO: Calculate sequence counters.
  }
  return aggr;
}

} // namespace howling
