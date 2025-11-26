#include <exception>
#include <filesystem>
#include <iostream>
#include <ranges>

#include "absl/flags/flag.h"
#include "cli/colorize.h"
#include "cli/printing.h"
#include "containers/vector.h"
#include "data/aggregate.h"
#include "data/analyzer.h"
#include "data/load_analyzer.h"
#include "data/stock.pb.h"
#include "data/utilities.h"
#include "howling_tools/init.h"
#include "howling_tools/runfiles.h"
#include "time/conversion.h"

ABSL_FLAG(std::string, stock, "", "Stock symbol to evaluate against.");
ABSL_FLAG(std::string, analyzer, "", "Name of an analyzer to evaluate.");
ABSL_FLAG(
    double,
    initial_funds,
    200'000,
    "Available funds at begining of evaluation.");

namespace howling {
namespace {

namespace fs = ::std::filesystem;

void run() {
  if (absl::GetFlag(FLAGS_analyzer).empty()) {
    throw std::runtime_error("Must specify an analyzer.");
  }
  stock::Symbol symbol = get_stock_symbol(absl::GetFlag(FLAGS_stock));
  auto anal = load_analyzer(absl::GetFlag(FLAGS_analyzer));

  fs::path data_directory = runfile(
      get_history_file_path(symbol, /*date=*/"").parent_path().string());
  auto files = fs::directory_iterator(data_directory) |
      std::ranges::to<vector<fs::directory_entry>>();
  std::ranges::sort(
      files, [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return std::ranges::lexicographical_compare(
            a.path().filename(), b.path().filename());
      });

  trading_state state{
      .available_stocks = vector<stock::Symbol>{{symbol}},
      .initial_funds = absl::GetFlag(FLAGS_initial_funds),
      .available_funds = absl::GetFlag(FLAGS_initial_funds),
  };
  int total_sales = 0;
  int total_profitable_sales = 0;
  for (const fs::directory_entry& file : files) {
    double days_starting_funds = state.available_funds;
    int sales = 0;
    int profitable_sales = 0;

    stock::History history = read_history(file.path());
    for (const Candle& candle : history.candles()) {
      state.time_now =
          to_std_chrono(candle.opened_at()) + to_std_chrono(candle.duration());
      add_next_minute(state.market[symbol], candle);
      decision d = anal->analyze(symbol, state);
      if (d.act == action::BUY) {
        state.available_funds -= candle.close();
        state.positions[symbol].push_back(
            {.symbol = symbol, .price = candle.close(), .quantity = 1});
      } else if (d.act == action::SELL && !state.positions[symbol].empty()) {
        for (const trading_state::position& p : state.positions[symbol]) {
          ++sales;
          if (p.price < candle.close()) ++profitable_sales;
          state.available_funds += candle.close() * p.quantity;
        }
        state.positions[symbol].clear();
      }
    }
    double profit = state.available_funds + state.total_positions_value() -
        days_starting_funds;
    std::cout << file.path().stem() << ":\n"
              << "  Sales:   " << sales << "\n"
              << "  +Sales:  " << profitable_sales << "\n"
              << "  $ Delta: "
              << colorize(
                     print_price(profit),
                     profit > 0 ? color::GREEN : color::RED)
              << "\n";

    total_sales += sales;
    total_profitable_sales += profitable_sales;
  }
  double profit = state.available_funds + state.total_positions_value() -
      absl::GetFlag(FLAGS_initial_funds);
  std::cout << "\nTotals:\n"
            << "  Sales:   " << total_sales << "\n"
            << "  +Sales:  " << total_profitable_sales << "\n"
            << "  $ Delta: "
            << colorize(
                   print_price(profit), profit > 0 ? color::GREEN : color::RED)
            << "\n";
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
