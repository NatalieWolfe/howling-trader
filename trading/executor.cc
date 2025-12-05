#include "trading/executor.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "trading/metrics.h"

ABSL_FLAG(bool, use_real_money, false, "Enables real money trading.");
ABSL_FLAG(
    double,
    max_fund_use,
    0.25,
    "Maximum percentage of initial funds to use at any one point.");
ABSL_FLAG(
    double,
    max_individual_buy_size,
    0.1,
    "Maximum percentage of available funds to use for any one buy order.");

namespace howling {
namespace {

void check_real_money_flag() {
  static int x = ([]() {
    if (absl::GetFlag(FLAGS_use_real_money)) {
      std::cout << "Confirm use of real money for trading? [y/N] ";
      std::string confirmation;
      std::cin >> confirmation;
      if (confirmation != "y" && confirmation != "Y") {
        std::cout << "Real money not confirmed." << std::endl;
        std::exit(1);
      }
      std::cout << "WARNING: USING REAL MONEY" << std::endl;
      std::cerr << "j/k not implemented yet lolol" << std::endl;
      std::exit(1);
    }
    return 0;
  })();
}

int get_buy_quantity(const trading_state& state, double price) {
  double max_buy_price =
      state.available_funds * absl::GetFlag(FLAGS_max_individual_buy_size);
  double utilized_funds =
      std::max(0.0, state.initial_funds - state.available_funds);
  double available_for_use =
      (state.initial_funds * absl::GetFlag(FLAGS_max_fund_use)) -
      utilized_funds;
  double purchase_target = std::min(max_buy_price, available_for_use);

  return std::max<int>(0, purchase_target / price);
}

} // namespace

executor::executor(trading_state& state) : _state{state} {
  check_real_money_flag();

  // TODO: Generate schwab connection.
}

void executor::buy(stock::Symbol symbol, metrics& m) {
  auto market_itr = _market.find(symbol);
  if (market_itr == _market.end()) return;

  double share_price = market_itr->second.ask();
  int buy_quantity = get_buy_quantity(_state, share_price);
  if (buy_quantity == 0) return;
  m.last_buy_price = share_price;

  if (!absl::GetFlag(FLAGS_use_real_money)) {
    _state.positions[symbol].push_back(
        {.symbol = symbol, .price = share_price, .quantity = buy_quantity});
    _state.available_funds -= share_price * buy_quantity;
    return;
  }
  std::cerr << "Buying with real money is not implemented.";
  std::exit(1); // Not implemented.
}

void executor::sell(stock::Symbol symbol, metrics& m) {
  auto market_itr = _market.find(symbol);
  if (market_itr == _market.end()) return;

  double share_price = market_itr->second.bid();
  int sell_quantity = 0;
  for (const trading_state::position& position : _state.positions[symbol]) {
    ++m.sales;
    if (share_price > position.price) ++m.profitable_sales;
    sell_quantity += position.quantity;
  }
  if (sell_quantity == 0) return;

  if (!absl::GetFlag(FLAGS_use_real_money)) {
    _state.positions[symbol].clear();
    _state.available_funds += share_price * sell_quantity;
    return;
  }
  std::cerr << "Selling with real money is not implemented.";
  std::exit(1); // Not implemented.
}

void executor::update_market(const Market& market) {
  Market& cached = _market[market.symbol()];
  cached.set_symbol(market.symbol());
  if (market.bid_lots() > 0) {
    cached.set_bid(market.bid());
    cached.set_bid_lots(market.bid_lots());
  }
  if (market.ask_lots() > 0) {
    cached.set_ask(market.ask());
    cached.set_ask_lots(market.ask_lots());
  }
  if (market.last_lots() > 0) {
    cached.set_last(market.last());
    cached.set_last_lots(market.last_lots());
  }
  *cached.mutable_emitted_at() = market.emitted_at();
}

} // namespace howling
