#pragma once

#include <future>
#include <generator>

#include "data/candle.pb.h"
#include "data/stock.pb.h"

namespace howling {

class database {
public:
  virtual ~database() = default;

  virtual std::future<void>
  save(stock::Symbol symbol, const Candle& candle) = 0;

  virtual std::generator<Candle> read_candles(stock::Symbol symbol) = 0;
};

} // namespace howling
