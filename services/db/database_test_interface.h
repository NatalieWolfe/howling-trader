#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "absl/flags/flag.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "services/database.h"
#include "services/db/environment.h"
#include "services/mock_security.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {

class DatabaseTest : public testing::Test {
protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_db_encryption_key_name, "test_key_name");
    clear_database();
  }

  virtual database& db() = 0;
  virtual void clear_database() = 0;

  void upgrade_schema() { db().upgrade_schema("").get(); }
  void check_schema_version() { db().check_schema_version().get(); }

  void save_candle(stock::Symbol symbol, const Candle& candle) {
    db().save(symbol, candle).get();
  }
  void save_market(const Market& market) { db().save(market).get(); }
  void save_trade(const trading::TradeRecord& trade) {
    db().save_trade(trade).get();
  }
  void
  save_refresh_token(std::string_view service_name, std::string_view token) {
    db().save_refresh_token(service_name, token).get();
  }

  std::generator<Candle> read_candles(stock::Symbol symbol) {
    return db().read_candles(symbol);
  }
  std::generator<Market> read_market(stock::Symbol symbol) {
    return db().read_market(symbol);
  }
  std::generator<trading::TradeRecord> read_trades(stock::Symbol symbol) {
    return db().read_trades(symbol);
  }

  std::optional<storage::auth_token>
  get_auth_token(std::string_view service_name) {
    return db().get_auth_token(service_name).get();
  }
  void save_notice_token(
      std::string_view service_name, std::string_view notice_token) {
    db().save_notice_token(service_name, notice_token).get();
  }

  mock_security_client _mock_security;
};

// TODO: Add `check_schema_version` tests. Requires having a database reset
// command because PG maintains state between tests.

#define DATABASE_TEST(FIXTURE_CLASS)                                           \
  TEST_F(FIXTURE_CLASS, CanSaveCandles) {                                      \
    upgrade_schema();                                                          \
    EXPECT_NO_THROW(save_candle(stock::NVDA, Candle::default_instance()));     \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, SavedCandlesAreReadable) {                             \
    upgrade_schema();                                                          \
    Candle candle;                                                             \
    candle.set_open(1.0);                                                      \
    candle.set_close(2.0);                                                     \
    candle.set_high(3.0);                                                      \
    candle.set_low(4.0);                                                       \
    candle.set_volume(5);                                                      \
    candle.mutable_opened_at()->set_seconds(6);                                \
    candle.mutable_duration()->set_seconds(7);                                 \
    save_candle(stock::NVDA, candle);                                          \
    int count = 0;                                                             \
    for (const Candle& found_candle : read_candles(stock::NVDA)) {             \
      ++count;                                                                 \
      EXPECT_THAT(                                                             \
          found_candle,                                                        \
          testing::AllOf(                                                      \
              testing::Property("open", &Candle::open, candle.open()),         \
              testing::Property("close", &Candle::close, candle.close()),      \
              testing::Property("high", &Candle::high, candle.high()),         \
              testing::Property("low", &Candle::low, candle.low()),            \
              testing::Property("volume", &Candle::volume, candle.volume()),   \
              testing::Property(                                               \
                  "opened_at",                                                 \
                  &Candle::opened_at,                                          \
                  testing::Property(                                           \
                      "seconds",                                               \
                      &::google::protobuf::Timestamp::seconds,                 \
                      candle.opened_at().seconds())),                          \
              testing::Property(                                               \
                  "duration",                                                  \
                  &Candle::duration,                                           \
                  testing::Property(                                           \
                      "seconds",                                               \
                      &::google::protobuf::Duration::seconds,                  \
                      candle.duration().seconds()))));                         \
    }                                                                          \
    EXPECT_EQ(count, 1);                                                       \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, CanSaveMarket) {                                       \
    upgrade_schema();                                                          \
    EXPECT_NO_THROW(save_market(Market::default_instance()));                  \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, SavedMarketHistoryIsReadable) {                        \
    upgrade_schema();                                                          \
    Market market;                                                             \
    market.set_symbol(stock::NVDA);                                            \
    market.set_bid(1.0);                                                       \
    market.set_bid_lots(2);                                                    \
    market.set_ask(3.0);                                                       \
    market.set_ask_lots(4);                                                    \
    market.set_last(5.0);                                                      \
    market.set_last_lots(6);                                                   \
    market.mutable_emitted_at()->set_seconds(7);                               \
    save_market(market);                                                       \
    int count = 0;                                                             \
    for (const Market& found_market : read_market(stock::NVDA)) {              \
      ++count;                                                                 \
      EXPECT_THAT(                                                             \
          found_market,                                                        \
          testing::AllOf(                                                      \
              testing::Property("bid", &Market::bid, market.bid()),            \
              testing::Property(                                               \
                  "bid_lots", &Market::bid_lots, market.bid_lots()),           \
              testing::Property("ask", &Market::ask, market.ask()),            \
              testing::Property(                                               \
                  "ask_lots", &Market::ask_lots, market.ask_lots()),           \
              testing::Property("last", &Market::last, market.last()),         \
              testing::Property(                                               \
                  "last_lots", &Market::last_lots, market.last_lots()),        \
              testing::Property(                                               \
                  "emitted_at",                                                \
                  &Market::emitted_at,                                         \
                  testing::Property(                                           \
                      "seconds",                                               \
                      &::google::protobuf::Timestamp::seconds,                 \
                      market.emitted_at().seconds()))));                       \
    }                                                                          \
    EXPECT_EQ(count, 1);                                                       \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, CanSaveTrade) {                                        \
    upgrade_schema();                                                          \
    trading::TradeRecord trade;                                                \
    trade.set_symbol(stock::NVDA);                                             \
    trade.set_action(trading::BUY);                                            \
    trade.set_price(100.0);                                                    \
    trade.set_quantity(10);                                                    \
    trade.set_confidence(0.9);                                                 \
    trade.set_dry_run(true);                                                   \
    EXPECT_NO_THROW(save_trade(trade));                                        \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, SavedTradesAreReadable) {                              \
    upgrade_schema();                                                          \
    trading::TradeRecord trade;                                                \
    trade.set_symbol(stock::NVDA);                                             \
    trade.set_action(trading::SELL);                                           \
    trade.set_price(200.0);                                                    \
    trade.set_quantity(20);                                                    \
    trade.set_confidence(0.95);                                                \
    trade.set_dry_run(false);                                                  \
    save_trade(trade);                                                         \
    int count = 0;                                                             \
    for (const trading::TradeRecord& found_trade : read_trades(stock::NVDA)) { \
      ++count;                                                                 \
      EXPECT_EQ(found_trade.symbol(), stock::NVDA);                            \
      EXPECT_EQ(found_trade.action(), trading::SELL);                          \
      EXPECT_DOUBLE_EQ(found_trade.price(), 200.0);                            \
      EXPECT_EQ(found_trade.quantity(), 20);                                   \
      EXPECT_DOUBLE_EQ(found_trade.confidence(), 0.95);                        \
      EXPECT_FALSE(found_trade.dry_run());                                     \
    }                                                                          \
    EXPECT_EQ(count, 1);                                                       \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, CanSaveRefreshToken) {                                 \
    upgrade_schema();                                                          \
    EXPECT_NO_THROW(save_refresh_token("schwab", "token"));                    \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, SavedRefreshTokenIsReadable) {                         \
    upgrade_schema();                                                          \
    std::string service = "schwab";                                            \
    std::string token = "my_secret_token";                                     \
    std::string encrypted = "vault:v1:encrypted";                              \
    EXPECT_CALL(_mock_security, encrypt("test_key_name", token))               \
        .WillOnce(testing::Return(encrypted));                                 \
    EXPECT_CALL(_mock_security, decrypt("test_key_name", encrypted))           \
        .WillOnce(testing::Return(token));                                     \
    save_refresh_token(service, token);                                        \
    EXPECT_EQ(get_auth_token(service)->refresh_token, token);                  \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, ReadingMissingRefreshTokenReturnsEmpty) {              \
    upgrade_schema();                                                          \
    EXPECT_EQ(get_auth_token("missing_service"), std::nullopt);                \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, CanRecordAndReadNotificationTime) {                    \
    upgrade_schema();                                                          \
    std::string service = "schwab";                                            \
    EXPECT_CALL(_mock_security, encrypt("test_key_name", "token"))             \
        .WillOnce(testing::Return("encrypted"));                               \
    save_refresh_token(service, "token");                                      \
    EXPECT_FALSE(get_auth_token(service)->last_notified_at.has_value());       \
    save_notice_token(service, "notice token");                                \
    EXPECT_THAT(                                                               \
        get_auth_token(service),                                               \
        testing::Optional(                                                     \
            testing::AllOf(                                                    \
                testing::Field(                                                \
                    "notice_token",                                            \
                    &storage::auth_token::notice_token,                        \
                    testing::Optional(testing::Eq("notice token"))),           \
                testing::Field(                                                \
                    "last_notified_at",                                        \
                    &storage::auth_token::last_notified_at,                    \
                    testing::Optional(                                         \
                        testing::Gt(                                           \
                            std::chrono::system_clock::now() -                 \
                            std::chrono::seconds(10)))))));                    \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, ReplayReturnsCandlesAndMarketsInTimeOrder) {           \
    upgrade_schema();                                                          \
    std::chrono::time_point start = std::chrono::system_clock::now();          \
    std::chrono::time_point first_candle_at = start + std::chrono::minutes(1); \
    std::chrono::time_point market_at = start + std::chrono::minutes(2);       \
    std::chrono::time_point second_candle_at =                                 \
        start + std::chrono::minutes(3);                                       \
    Candle first_candle;                                                       \
    first_candle.set_open(100.0);                                              \
    first_candle.set_close(110.0);                                             \
    first_candle.set_high(115.0);                                              \
    first_candle.set_low(95.0);                                                \
    first_candle.set_volume(123);                                              \
    *first_candle.mutable_opened_at() = to_proto(first_candle_at);             \
    *first_candle.mutable_duration() = to_proto(std::chrono::minutes(1));      \
    save_candle(stock::NVDA, first_candle);                                    \
    Market market;                                                             \
    market.set_symbol(stock::NVDA);                                            \
    market.set_bid(101.0);                                                     \
    market.set_bid_lots(2);                                                    \
    market.set_ask(103.0);                                                     \
    market.set_ask_lots(4);                                                    \
    market.set_last(102.0);                                                    \
    market.set_last_lots(6);                                                   \
    *market.mutable_emitted_at() = to_proto(market_at);                        \
    save_market(market);                                                       \
    Candle second_candle;                                                      \
    second_candle.set_open(120.0);                                             \
    second_candle.set_close(130.0);                                            \
    second_candle.set_high(135.0);                                             \
    second_candle.set_low(118.0);                                              \
    second_candle.set_volume(456);                                             \
    *second_candle.mutable_opened_at() = to_proto(second_candle_at);           \
    *second_candle.mutable_duration() = to_proto(std::chrono::minutes(1));     \
    save_candle(stock::NVDA, second_candle);                                   \
    std::vector<std::variant<Candle, Market>> replayed;                        \
    for (const auto& record :                                                  \
         db().replay(stock::NVDA, start, start + std::chrono::minutes(10))) {  \
      replayed.push_back(record);                                              \
    }                                                                          \
    EXPECT_THAT(                                                               \
        replayed,                                                              \
        testing::ElementsAre(                                                  \
            testing::VariantWith<Candle>(testing::Property(                    \
                &Candle::open, testing::DoubleEq(first_candle.open()))),       \
            testing::VariantWith<Market>(testing::Property(                    \
                &Market::bid, testing::DoubleEq(market.bid()))),               \
            testing::VariantWith<Candle>(testing::Property(                    \
                &Candle::open, testing::DoubleEq(second_candle.open())))));    \
  }

} // namespace howling
