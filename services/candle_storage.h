#pragma once

#include <generator>
#include <utility>

#include "containers/buffered_stream.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "services/database.h"

namespace howling {

class candle_storage {
public:
  candle_storage(database& db) : _db{db} {}

  void receive(std::generator<std::pair<stock::Symbol, Candle>> candle_stream);

private:
  database& _db;
};

} // namespace howling
