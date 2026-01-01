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

  std::generator<Candle> read_candles(stock::Symbol symbol) override;
  std::generator<Market> read_market(stock::Symbol symbol) override;

private:
  void _check(int code, std::source_location loc = {});

  sqlite3* _db = nullptr;
};

} // namespace howling
