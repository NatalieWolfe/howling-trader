#pragma once

#include <future>
#include <generator>
#include <optional>
#include <string>
#include <string_view>

#include "services/database.h"
#include "services/db/schema/auth_token.h"
#include "gmock/gmock.h"

namespace howling {

class mock_database : public database {
public:
  MOCK_METHOD(
      std::future<void>,
      upgrade_schema,
      (std::string_view app_db_user),
      (override));
  MOCK_METHOD(std::future<void>, check_schema_version, (), (override));

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
      std::future<std::optional<storage::auth_token>>,
      get_auth_token,
      (std::string_view service_name),
      (override));
  MOCK_METHOD(
      std::future<void>,
      save_notice_token,
      (std::string_view service_name, std::string_view notice_token),
      (override));
};

} // namespace howling
