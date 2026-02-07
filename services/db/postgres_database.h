#pragma once

#include <future>
#include <generator>
#include <memory>
#include <string_view>

#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "services/database.h"

namespace howling {

struct postgres_options {
  std::string_view host;
  std::string_view port;
  std::string_view user;
  std::string_view password;
  std::string_view dbname;
};

class postgres_database : public database {
public:
  postgres_database(postgres_options options);
  ~postgres_database();

  std::future<void> save(stock::Symbol symbol, const Candle& candle) override;
  std::future<void> save(const Market& market) override;

  std::generator<Candle> read_candles(stock::Symbol symbol) override;
  std::generator<Market> read_market(stock::Symbol symbol) override;

  // Clears all data from the database.
  void clear_all();

private:
  struct implementation;
  std::unique_ptr<implementation> _implementation;
};

} // namespace howling
