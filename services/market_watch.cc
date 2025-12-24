#include "services/market_watch.h"

#include <chrono>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "api/schwab.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"

ABSL_FLAG(
    bool,
    prefetch_history,
    true,
    "Set to true to fetch historic data before watching current market "
    "movements.");

namespace howling {
namespace {

std::vector<std::pair<stock::Symbol, Candle>>
prefetch_history(std::span<const stock::Symbol> symbols) {
  auto now = std::chrono::system_clock::now();
  schwab::api_connection conn;
  std::vector<std::pair<stock::Symbol, Candle>> all_candles;
  for (stock::Symbol symbol : symbols) {
    for (Candle& candle : conn.get_history(symbol, {.end_date = now})) {
      all_candles.push_back(std::make_pair(symbol, std::move(candle)));
    }
  }
  std::ranges::sort(
      all_candles,
      [](const std::pair<stock::Symbol, Candle>& a,
         const std::pair<stock::Symbol, Candle>& b) {
        // Intermediate variables needed for std::tie.
        auto a_seconds = a.second.opened_at().seconds();
        auto a_nanos = a.second.opened_at().nanos();
        auto a_symbol = a.first;
        auto b_seconds = b.second.opened_at().seconds();
        auto b_nanos = b.second.opened_at().nanos();
        auto b_symbol = b.first;
        return (
            std::tie(a_seconds, a_nanos, a_symbol) <
            std::tie(b_seconds, b_nanos, b_symbol));
      });
  return all_candles;
}

} // namespace

market_watch::~market_watch() {
  _schwab.stop();
}

void market_watch::start(std::span<const stock::Symbol> symbols) {
  if (absl::GetFlag(FLAGS_prefetch_history)) {
    for (auto&& [symbol, candle] : prefetch_history(symbols)) {
      _candles.push_back(std::make_pair(symbol, std::move(candle)));
    }
  }

  _schwab.on_chart([this](stock::Symbol symbol, Candle candle) {
    _candles.push_back(std::make_pair(symbol, std::move(candle)));
  });
  _schwab.on_market([this](stock::Symbol, Market market) {
    _market.push_back(std::move(market));
  });
  _schwab.start([&]() {
    for (stock::Symbol symbol : symbols) _schwab.add_symbol(symbol);
  });
}

} // namespace howling
