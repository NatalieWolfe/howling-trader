#include "cli/printing.h"

#include <algorithm>
#include <exception>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>

#include "absl/strings/str_cat.h"
#include "cli/colorize.h"
#include "data/candle.pb.h"

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

std::string print_candle(const Candle& candle, const print_extents& extents) {
  color c = candle.open() < candle.close() ? color::GREEN : color::RED;

  double usable_width = get_terminal_width() * extents.max_width_usage;
  double gap = extents.max - extents.min;
  double scaler = (usable_width) / gap;

  int body_min = std::floor(
      (std::min(candle.open(), candle.close()) - extents.min) * scaler);
  int body_max = std::floor(
      (std::max(candle.open(), candle.close()) - extents.min) * scaler);
  int low_wick = std::floor((candle.low() - extents.min) * scaler);
  int high_wick = std::floor((candle.high() - extents.min) * scaler);

  return absl::StrCat(
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
      std::string(static_cast<int>(usable_width) - high_wick, ' '));
}

} // namespace howling
