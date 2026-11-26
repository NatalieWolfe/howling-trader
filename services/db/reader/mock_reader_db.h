#pragma once

#include <vector>

#include "services/db/reader/reader_db.h"
#include "gmock/gmock.h"

namespace howling {

/**
 * @brief GoogleMock implementation of reader_db for unit tests.
 */
class mock_reader_db : public reader_db {
public:
  mock_reader_db() {
    ON_CALL(*this, is_healthy()).WillByDefault(::testing::Return(true));
  }

  MOCK_METHOD(bool, is_healthy, (), (const, override));
  MOCK_METHOD(void, reset, (), (override));

  MOCK_METHOD(
      candle_page, read_candles, (const candle_query& query), (override));
  MOCK_METHOD(
      market_page, read_market_data, (const market_query& query), (override));
  MOCK_METHOD(trade_page, read_trades, (const trade_query& query), (override));
  MOCK_METHOD(
      std::vector<trading_summary_row>,
      read_trading_summary,
      (const summary_query& query),
      (override));
  MOCK_METHOD(symbols_metadata, read_symbols_metadata, (), (override));
  MOCK_METHOD(std::vector<action_info>, read_actions_metadata, (), (override));
  MOCK_METHOD(schema_metadata, read_schema_metadata, (), (override));
};

} // namespace howling
