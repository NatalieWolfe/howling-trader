#include "cli/printing.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>

#include "absl/strings/str_cat.h"
#include "cli/colorize.h"
#include "data/candle.pb.h"
#include "time/conversion.h"
#include "trading/metrics.h"

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
    const metrics& m,
    const Candle& candle,
    const print_extents& candle_extents) {
  using namespace ::std::chrono;
  zoned_time opened_at{current_zone(), to_std_chrono(candle.opened_at())};
  hh_mm_ss time_of_day{floor<seconds>(
      opened_at.get_local_time() - floor<days>(opened_at.get_local_time()))};
  const bool is_ref_point = time_of_day.minutes().count() % 15 == 0;
  std::string prefix =
      is_ref_point ? std::format(" {} | ", time_of_day) : "          | ";

  color c = candle.open() < candle.close() ? color::GREEN : color::RED;

  double usable_width = get_terminal_width() * candle_extents.max_width_usage;
  double gap = candle_extents.max - candle_extents.min;
  double scaler = (usable_width) / gap;

  int body_min = std::floor(
      (std::min(candle.open(), candle.close()) - candle_extents.min) * scaler);
  int body_max = std::floor(
      (std::max(candle.open(), candle.close()) - candle_extents.min) * scaler);
  int low_wick = std::floor((candle.low() - candle_extents.min) * scaler);
  int high_wick = std::floor((candle.high() - candle_extents.min) * scaler);

  std::string suffix;
  if (d.act == action::BUY) {
    suffix = std::format(
        "{} - Buy ({:.2f})",
        colorize(print_price(candle.close()), color::RED),
        d.confidence);
  } else if (d.act == action::SELL) {
    suffix = std::format(
        "{} - Sell ({:.2f}: Δ{:.2f})",
        colorize(print_price(candle.close()), color::GREEN),
        d.confidence,
        candle.close() - m.last_buy_price);
  } else if (candle.low() == m.min) {
    suffix = colorize(print_price(candle.low()), color::RED);
  } else if (candle.high() == m.max) {
    suffix = colorize(print_price(candle.high()), color::GREEN);
  } else if (is_ref_point) {
    suffix = colorize(print_price(candle.close()), color::GRAY);
  }

  return absl::StrCat(
      prefix,
      std::string(low_wick, ' '),
      colorize(
          absl::StrCat(
              std::views::repeat(WICK, body_min - low_wick) | std::views::join |
                  std::ranges::to<std::string>(),
              std::views::repeat(BLOCK, body_max - body_min) |
                  std::views::join | std::ranges::to<std::string>(),
              std::views::repeat(WICK, high_wick - body_max) |
                  std::views::join | std::ranges::to<std::string>()),
          c),
      std::string(static_cast<int>(usable_width) - high_wick, ' '),
      " | ",
      suffix);
}

std::string print_metrics(const metrics& m) {
  double profit = m.available_funds + m.assets_value - m.initial_funds;
  return absl::StrCat(
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
}

} // namespace howling
