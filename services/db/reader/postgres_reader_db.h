#pragma once

#include <memory>
#include <vector>

#include "services/db/pg/connection.h"
#include "services/db/reader/reader_db.h"

namespace howling {

/**
 * @brief Read-only PostgreSQL implementation of reader_db.
 *
 * Every connection enforces default_transaction_read_only = on and
 * statement_timeout = '10s'. All queries execute via one-shot PQexecParams
 * binary queries. Thread-compatible; use reader_db_pool for concurrent access.
 */
class postgres_reader_db : public reader_db {
public:
  explicit postgres_reader_db(postgres_options options);
  ~postgres_reader_db() override;

  postgres_reader_db(const postgres_reader_db&) = delete;
  postgres_reader_db& operator=(const postgres_reader_db&) = delete;

  [[nodiscard]] bool is_healthy() const override;
  void reset() override;

  [[nodiscard]] candle_page read_candles(const candle_query& query) override;
  [[nodiscard]] market_page
  read_market_data(const market_query& query) override;
  [[nodiscard]] trade_page read_trades(const trade_query& query) override;
  [[nodiscard]] std::vector<trading_summary_row>
  read_trading_summary(const summary_query& query) override;
  [[nodiscard]] symbols_metadata read_symbols_metadata() override;
  [[nodiscard]] std::vector<action_info> read_actions_metadata() override;
  [[nodiscard]] schema_metadata read_schema_metadata() override;

private:
  friend struct postgres_reader_db_test_peer;

  PGconn& _connection();

  struct implementation;
  std::unique_ptr<implementation> _implementation;
};

} // namespace howling
