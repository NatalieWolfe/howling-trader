#include "data/aggregate.h"

#include <algorithm>
#include <span>
#include <vector>

#include "absl/flags/flag.h"
#include "containers/vector.h"
#include "data/candle.pb.h"

ABSL_FLAG(
    int,
    fast_exponential_average_period,
    12,
    "Period for the fast moving average.");
ABSL_FLAG(
    int,
    slow_exponential_average_period,
    26,
    "Period for the slow moving average.");
ABSL_FLAG(int, macd_signal_line, 9, "Period for the MACD signal line.");

namespace howling {
namespace {

window blank_window() {
  return {
      .candle = Candle::default_instance(),
      .count = 0,
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

const window* maybe_get_previous(const vector<window>& windows, int offset) {
  return windows.size() < static_cast<size_t>(offset) ? nullptr
                                                      : &windows(-offset);
}

double
exponential_moving_average(double price, double previous_ema, int period) {
  double k = 2.0 / (period + 1.0);
  return (price * k) + (previous_ema * (1.0 - k));
}

void calculate_macd(window& w, const window* previous) {
  if (previous) {
    w.fast_exponential_average = exponential_moving_average(
        w.candle.close(),
        previous->fast_exponential_average,
        absl::GetFlag(FLAGS_fast_exponential_average_period));
    w.slow_exponential_average = exponential_moving_average(
        w.candle.close(),
        previous->slow_exponential_average,
        absl::GetFlag(FLAGS_slow_exponential_average_period));
    w.macd_fast_line = w.fast_exponential_average - w.slow_exponential_average;
    w.macd_signal_line = exponential_moving_average(
        w.macd_fast_line,
        previous->macd_signal_line,
        absl::GetFlag(FLAGS_macd_signal_line));
  } else {
    w.fast_exponential_average = w.moving_average;
    w.slow_exponential_average = w.moving_average;
    w.macd_fast_line = 0.0;
    w.macd_signal_line = 0.0;
  }
}

window to_window(const Candle& candle, const window* previous) {
  double body_high = std::max(candle.open(), candle.close());
  double body_low = std::min(candle.open(), candle.close());
  window w{
      .candle = candle,
      .count = 1,
      .green_body = candle.close() > candle.open(),
      .body_high = body_high,
      .body_low = body_low,
      .price_delta = body_high - body_low,
      .upper_wick_length = candle.high() - body_high,
      .lower_wick_length = body_low - candle.low(),
      .moving_average = candle.close(),
      .stddev = 0,
      .upper_bollinger_band = 0,
      .lower_bollinger_band = 0,
      .green_sequence = false,
      .setup_counter = 0,
      .countdown_counter = 0,
  };
  w.total_wick_length = w.upper_wick_length + w.lower_wick_length;
  w.wick_body_ratio = w.total_wick_length / w.price_delta;

  calculate_macd(w, previous);

  return w;
}

void add_next_window(window& a, const window& b) {
  a.count += b.count;
  a.candle.set_close(b.candle.close());
  a.candle.set_high(std::max(a.candle.high(), b.candle.high()));
  a.candle.set_low(std::min(a.candle.low(), b.candle.low()));
  a.candle.set_volume(a.candle.volume() + b.candle.volume());
  // TODO: Add b.duration to a.duration

  window combined = to_window(a.candle, nullptr);
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

window do_aggregate(
    std::span<const window> one_minute_windows, const window* previous) {
  window w = blank_window();
  for (const window& new_window : one_minute_windows) {
    add_next_window(w, new_window);
  }
  w.moving_average /= one_minute_windows.size();
  double sq_diff_sum = 0;
  for (const window& new_window : one_minute_windows) {
    double diff = new_window.candle.close() - w.moving_average;
    sq_diff_sum += diff * diff;
  }
  w.stddev = std::sqrt(sq_diff_sum / one_minute_windows.size());
  w.upper_bollinger_band = w.moving_average + (2.0 * w.stddev);
  w.lower_bollinger_band = w.moving_average - (2.0 * w.stddev);

  calculate_macd(w, previous);

  return w;
}

} // namespace

aggregations aggregate(const vector<Candle>& one_minute_candles) {
  aggregations aggr;
  for (const Candle& candle : one_minute_candles) {
    add_next_minute(aggr, candle);
  }
  return aggr;
}

void add_next_minute(aggregations& aggr, const Candle& candle) {
  // TODO: Calculate sequence counters.
  aggr.one_minute.push_back(
      to_window(candle, maybe_get_previous(aggr.one_minute, 1)));

  // For multi-minute aggregations, we use an offset equal to the window size
  // (e.g., 5 or 20) when retrieving the previous window for EMA/MACD
  // calculations. This ensures that the EMA state is updated only once per
  // "full" window of data, preventing the same minutes from being counted
  // multiple times in the exponential moving average sequence.
  aggr.five_minute.push_back(do_aggregate(
      aggr.one_minute.last_n(5), maybe_get_previous(aggr.five_minute, 5)));
  aggr.twenty_minute.push_back(do_aggregate(
      aggr.one_minute.last_n(20), maybe_get_previous(aggr.twenty_minute, 20)));
}

} // namespace howling
