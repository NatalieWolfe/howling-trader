#pragma once

#include <future>
#include <generator>
#include <memory>
#include <string>
#include <string_view>

#include "data/analyzer.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "services/db/schema/auth_token.h"
#include "services/service_base.h"

namespace howling {

class database : public service {
public:
  virtual ~database() = default;

  /**
   * @brief Upgrades the database schema to the most recent version.
   *
   * If the schema is already up to date, this is a no-op.
   *
   * @param app_db_user The username of the non-admin database user.
   */
  virtual std::future<void> upgrade_schema(std::string_view app_db_user) = 0;

  /**
   * @brief Asserts that the schema of the database matches what is expected by
   * this connection.
   *
   * @throws A runtime error if the schema version is the expected version.
   */
  virtual std::future<void> check_schema_version() = 0;

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

  /**
   * @brief Retrieves the authorization information saved for the given service.
   */
  virtual std::future<std::optional<storage::auth_token>>
  get_auth_token(std::string_view service_name) = 0;
  /**
   * @brief Saves the token for the authentication notification.
   *
   * This also updates the last_notified_at timestamp.
   */
  virtual std::future<void> save_notice_token(
      std::string_view service_name, std::string_view notice_token) = 0;
};

} // namespace howling
