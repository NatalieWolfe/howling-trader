#pragma once

#include <future>
#include <generator>
#include <source_location>

#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "services/database.h"
#include "sqlite3.h"

namespace howling {

class sqlite_database : public database {
public:
  sqlite_database();
  ~sqlite_database();

  std::future<void> save(stock::Symbol symbol, const Candle& candle) override;
  std::future<void> save(const Market& market) override;
  std::future<void> save_trade(const trading::TradeRecord& trade) override;
  std::future<void> save_refresh_token(
      std::string_view service_name, std::string_view token) override;

  std::generator<Candle> read_candles(stock::Symbol symbol) override;
  std::generator<Market> read_market(stock::Symbol symbol) override;
  std::generator<trading::TradeRecord>
  read_trades(stock::Symbol symbol) override;
  std::future<std::string>
  read_refresh_token(std::string_view service_name) override;

  std::future<std::optional<std::chrono::system_clock::time_point>>
  get_last_notified_at(std::string_view service_name) override;
  std::future<void>
  update_last_notified_at(std::string_view service_name) override;

private:
  void _check(int code, std::source_location loc = {});

  sqlite3* _db = nullptr;
};

} // namespace howling
