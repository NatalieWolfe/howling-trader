#pragma once

#include <future>
#include <generator>
#include <string>
#include <string_view>

#include "gmock/gmock.h"
#include "services/database.h"

namespace howling {

class mock_database : public database {
public:
  MOCK_METHOD(
      std::future<void>,
      save,
      (stock::Symbol symbol, const Candle& candle),
      (override));
  MOCK_METHOD(std::future<void>, save, (const Market& market), (override));
  MOCK_METHOD(
      std::future<void>,
      save_trade,
      (const trading::TradeRecord& trade),
      (override));
  MOCK_METHOD(
      std::future<void>,
      save_refresh_token,
      (std::string_view service_name, std::string_view token),
      (override));

  MOCK_METHOD(
      std::generator<Candle>, read_candles, (stock::Symbol symbol), (override));
  MOCK_METHOD(
      std::generator<Market>, read_market, (stock::Symbol symbol), (override));
  MOCK_METHOD(
      std::generator<trading::TradeRecord>,
      read_trades,
      (stock::Symbol symbol),
      (override));
  MOCK_METHOD(
      std::future<std::string>,
      read_refresh_token,
      (std::string_view service_name),
      (override));

  MOCK_METHOD(
      (std::future<std::optional<std::chrono::system_clock::time_point>>),
      get_last_notified_at,
      (std::string_view service_name),
      (override));
  MOCK_METHOD(
      std::future<void>,
      update_last_notified_at,
      (std::string_view service_name),
      (override));
};

} // namespace howling
