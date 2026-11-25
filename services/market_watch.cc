#include "services/market_watch.h"

#include <chrono>
#include <mutex>
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
  {
    std::lock_guard lock{_state_mutex};
    _stopping = true;
  }
  _schwab.stop();
  std::unique_lock lock{_state_mutex};
  _stopped_cv.wait(lock, [this]() { return !_in_start; });
}

void market_watch::start(std::span<const stock::Symbol> symbols) {
  {
    std::lock_guard lock{_state_mutex};
    if (_stopping) return;
    _in_start = true;
  }

  struct start_guard {
    market_watch& owner;
    ~start_guard() {
      std::lock_guard lock{owner._state_mutex};
      owner._in_start = false;
      owner._stopped_cv.notify_all();
    }
  } guard{*this};

  if (absl::GetFlag(FLAGS_prefetch_history)) {
    for (auto&& [symbol, candle] : prefetch_history(symbols)) {
      {
        std::lock_guard lock{_state_mutex};
        if (_stopping) return;
      }
      _candles.push_back(std::make_pair(symbol, std::move(candle)));
    }
  }

  {
    std::lock_guard lock{_state_mutex};
    if (_stopping) return;
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
