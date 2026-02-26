#pragma once

#include <future>
#include <generator>
#include <string>
#include <string_view>

#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"

namespace howling {

class database {
public:
  virtual ~database() = default;

  virtual std::future<void>
  save(stock::Symbol symbol, const Candle& candle) = 0;
  virtual std::future<void> save(const Market& market) = 0;
  virtual std::future<void> save_trade(const trading::TradeRecord& trade) = 0;
  virtual std::future<void>
  save_refresh_token(std::string_view service_name, std::string_view token) = 0;

  virtual std::generator<Candle> read_candles(stock::Symbol symbol) = 0;
  virtual std::generator<Market> read_market(stock::Symbol symbol) = 0;
  virtual std::generator<trading::TradeRecord>
  read_trades(stock::Symbol symbol) = 0;
  virtual std::future<std::string>
  read_refresh_token(std::string_view service_name) = 0;

  virtual std::future<std::optional<std::chrono::system_clock::time_point>>
  get_last_notified_at(std::string_view service_name) = 0;
  virtual std::future<void>
  update_last_notified_at(std::string_view service_name) = 0;
};

} // namespace howling
