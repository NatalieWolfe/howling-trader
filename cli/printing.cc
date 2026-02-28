#include "cli/printing.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <numeric>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>
#include <utility>

#include "absl/strings/str_cat.h"
#include "cli/colorize.h"
#include "data/candle.pb.h"
#include "time/conversion.h"
#include "trading/metrics.h"
#include "trading/trading_state.h"

namespace howling {
namespace {

constexpr std::string_view BLOCK = "▓";
constexpr std::string_view WICK = "—";

int get_terminal_width() {
  ::winsize w{0};
  if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != 0) {
    throw std::runtime_error("Failed to read terminal width.");
  }
  return w.ws_col;
}

} // namespace

std::string print_candle(
    const decision& d,
    const std::optional<trading_state::position>& trade,
    const Candle& candle,
    const print_candle_parameters& params) {
  using namespace ::std::chrono;
  zoned_time opened_at{current_zone(), to_std_chrono(candle.opened_at())};
  hh_mm_ss time_of_day{floor<seconds>(
      opened_at.get_local_time() - floor<days>(opened_at.get_local_time()))};
  const bool is_ref_point = time_of_day.minutes().count() % 15 == 0;
  std::string prefix =
      is_ref_point ? std::format(" {} | ", time_of_day) : "          | ";

  color c = candle.open() < candle.close() ? color::GREEN : color::RED;

  double usable_width = get_terminal_width() * params.candle_width;
  double gap = params.candle_print_max - params.candle_print_min;
  double scaler = (usable_width) / gap;

  int body_min = std::floor(
      (std::min(candle.open(), candle.close()) - params.candle_print_min) *
      scaler);
  int body_max = std::floor(
      (std::max(candle.open(), candle.close()) - params.candle_print_min) *
      scaler);
  int low_wick = std::floor((candle.low() - params.candle_print_min) * scaler);
  int high_wick =
      std::floor((candle.high() - params.candle_print_min) * scaler);

  std::string suffix;
  double price = trade ? trade->price : candle.close();
  int quantity = trade ? trade->quantity : 0;
  if (d.act == action::BUY) {
    suffix = std::format(
        "{} x {} - Buy ({:.2f})",
        colorize(print_price(price), color::RED),
        quantity,
        d.confidence);
  } else if (d.act == action::SELL) {
    suffix = std::format(
        "{} x {} - Sell ({:.2f}: Δ{:.2f})",
        colorize(print_price(price), color::GREEN),
        quantity,
        d.confidence,
        price - params.last_buy_price);
  } else if (candle.low() == params.price_min) {
    suffix = colorize(print_price(candle.low()), color::RED);
  } else if (candle.high() == params.price_max) {
    suffix = colorize(print_price(candle.high()), color::GREEN);
  } else if (is_ref_point) {
    suffix = colorize(print_price(candle.close()), color::GRAY);
  }

  auto repeat = [](std::string_view sv, int n) {
    std::string s;
    if (n > 0) {
      s.reserve(sv.size() * n);
      for (int i = 0; i < n; ++i) s.append(sv);
    }
    return s;
  };

  return absl::StrCat(
      prefix,
      std::string(low_wick, ' '),
      colorize(
          absl::StrCat(
              repeat(WICK, body_min - low_wick),
              repeat(BLOCK, body_max - body_min),
              repeat(WICK, high_wick - body_max)),
          c),
      std::string(static_cast<int>(usable_width) - high_wick, ' '),
      " | ",
      suffix);
}

std::string print_metrics(const metrics& m) {
  double profit = m.available_funds + m.assets_value - m.initial_funds;
  std::string result = absl::StrCat(
      m.name,
      "\n  #Sales: ",
      m.sales,
      "\n  +Sales: ",
      m.profitable_sales,
      "\n  $Δ:     ",
      colorize(
          print_price(profit),
          profit > 0.0       ? color::GREEN
              : profit < 0.0 ? color::RED
                             : color::GRAY));

  if (!m.deltas.empty()) {
    auto sorted_deltas = m.deltas;
    std::ranges::sort(sorted_deltas);

    double min = sorted_deltas.front();
    double max = sorted_deltas.back();
    double median = sorted_deltas.size() % 2 == 0
        ? (sorted_deltas[sorted_deltas.size() / 2 - 1] +
           sorted_deltas[sorted_deltas.size() / 2]) /
            2.0
        : sorted_deltas[sorted_deltas.size() / 2];

    double sum =
        std::accumulate(sorted_deltas.begin(), sorted_deltas.end(), 0.0);
    double mean = sum / sorted_deltas.size();
    double sq_sum = 0.0;
    for (double d : sorted_deltas) { sq_sum += (d - mean) * (d - mean); }
    double stddev = std::sqrt(sq_sum / sorted_deltas.size());

    absl::StrAppend(
        &result,
        "\n  Min $Δ: ",
        print_price(min),
        "\n  Max $Δ: ",
        print_price(max),
        "\n  Med $Δ: ",
        print_price(median),
        "\n  Std $Δ: ",
        print_price(stddev));
  }

  return result;
}

} // namespace howling
