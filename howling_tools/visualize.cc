#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <ranges>
#include <sstream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "cli/colorize.h"
#include "cli/printing.h"
#include "data/analyzer.h"
#include "data/analyzers/noop.h"
#include "data/analyzers/zig_zag.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/text_format.h"
#include "howling_tools/init.h"
#include "howling_tools/runfiles.h"
#include "strings/format.h"
#include "time/conversion.h"

ABSL_FLAG(std::string, stock, "", "Stock symbol to visualize.");
ABSL_FLAG(std::string, date, "", "Day to visualize.");
ABSL_FLAG(std::string, analyzer, "", "Name of an analyzer to run.");

namespace howling {
namespace {

namespace fs = ::std::filesystem;

using ::google::protobuf::TextFormat;
using ::std::chrono::hh_mm_ss;
using ::std::chrono::system_clock;

const fs::path HISTORY_DIR = "howling-trader/data/history";

stock::History read_history(const fs::path& path) {
  std::ifstream stream(path);
  std::stringstream data;
  data << stream.rdbuf();

  stock::History history;
  if (!TextFormat::ParseFromString(data.str(), &history)) {
    throw std::runtime_error(
        absl::StrCat("Failed to parse contents of ", path.string()));
  }
  return history;
}

stock::Symbol get_stock_symbol() {
  std::string stock_name = absl::GetFlag(FLAGS_stock) |
      std::views::transform(to_upper) | std::ranges::to<std::string>();
  if (stock_name.empty()) {
    throw std::runtime_error("Must specify --stock flag.");
  }
  stock::Symbol symbol;
  if (!stock::Symbol_Parse(stock_name, &symbol)) {
    throw std::runtime_error(
        absl::StrCat("Unknown stock symbol: ", stock_name, "."));
  }
  return symbol;
}

std::unique_ptr<analyzer> load_analyzer(const stock::History& history) {
  std::string anal_name = absl::GetFlag(FLAGS_analyzer);
  if (anal_name.empty() || anal_name == "noop") {
    return std::make_unique<noop_analyzer>();
  }
  if (anal_name == "zig_zag" || anal_name == "optimal") {
    return std::make_unique<zig_zag_analyzer>(
        vector<Candle>{history.candles().begin(), history.candles().end()},
        zig_zag_analyzer::options{.threshold = 0.5});
  }
  throw std::runtime_error(absl::StrCat("Unknown analyzer: ", anal_name));
}

std::string print_price(double price) { return std::format("{:.2f}", price); }

void run() {
  stock::Symbol symbol = get_stock_symbol();
  std::string filename = absl::StrCat(absl::GetFlag(FLAGS_date), ".textproto");

  fs::path data_path =
      runfile((HISTORY_DIR / stock::Symbol_Name(symbol) / filename).string());
  if (!fs::exists(data_path)) {
    throw std::runtime_error(
        absl::StrCat("No data found at ", data_path.string()));
  }

  stock::History history = read_history(data_path);
  auto anal = load_analyzer(history);

  print_extents extents{
      .min = std::numeric_limits<double>::max(),
      .max = std::numeric_limits<double>::min(),
      .max_width_usage = 0.75};
  double min_close = std::numeric_limits<double>::max();
  double max_close = std::numeric_limits<double>::min();
  for (const Candle& candle : history.candles()) {
    extents.min = std::min(extents.min, candle.low());
    extents.max = std::max(extents.max, candle.high());
    min_close = std::min(min_close, candle.close());
    max_close = std::max(max_close, candle.close());
  }

  vector<Candle> observed;
  int buy_counter = 0;
  int sell_counter = 0;
  double last_buy = std::numeric_limits<double>::max();
  double profit = 0;
  for (const Candle& candle : history.candles()) {
    using namespace ::std::chrono;
    zoned_time opened_at{current_zone(), to_std_chrono(candle.opened_at())};
    hh_mm_ss time_of_day{floor<seconds>(
        opened_at.get_local_time() - floor<days>(opened_at.get_local_time()))};
    const bool is_ref_point = time_of_day.minutes().count() % 15 == 0;
    if (is_ref_point) {
      std::cout << ' ' << time_of_day << " | ";
    } else {
      std::cout << "          | ";
    }
    std::cout << print_candle(candle, extents) << " | ";

    observed.push_back(candle);
    decision d = anal->analyze(aggregate(observed));
    if (d.act == action::BUY) {
      ++buy_counter;
      last_buy = candle.low();
      std::cout << colorize(print_price(candle.low()), color::RED) << " - Buy ("
                << d.confidence << ")";
    } else if (d.act == action::SELL) {
      ++sell_counter;
      profit += candle.high() - last_buy;
      std::cout << colorize(print_price(candle.high()), color::GREEN)
                << " - Sell (" << d.confidence << "; "
                << print_price(candle.high() - last_buy) << ")";
    } else if (candle.close() == min_close) {
      std::cout << colorize(print_price(candle.close()), color::RED);
    } else if (candle.close() == max_close) {
      std::cout << colorize(print_price(candle.close()), color::GREEN);
    } else if (is_ref_point) {
      std::cout << colorize(print_price(candle.close()), color::GRAY);
    }
    std::cout << '\n';
  }
  if (buy_counter > 0) {
    std::cout << "\n"
              << "Buys:   " << buy_counter << "\n"
              << "Sells:  " << sell_counter << "\n"
              << "Profit: "
              << colorize(
                     print_price(profit),
                     profit > 0 ? color::GREEN : color::RED)
              << "\n";
  }
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);

  try {
    howling::run();
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  } catch (...) { std::cerr << "!!!! UNKNOWN ERROR THROWN !!!!" << std::endl; }
  return 1;
}
