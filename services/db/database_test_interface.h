#pragma once

#include <chrono>
#include <string>

#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling {

class DatabaseTest : public ::testing::Test {
protected:
  virtual void save_candle(stock::Symbol symbol, const Candle& candle) = 0;
  virtual void save_market(const Market& market) = 0;
  virtual void save_trade(const trading::TradeRecord& trade) = 0;
  virtual void
  save_refresh_token(std::string_view service_name, std::string_view token) = 0;

  virtual std::generator<Candle> read_candles(stock::Symbol symbol) = 0;
  virtual std::generator<Market> read_market(stock::Symbol symbol) = 0;
  virtual std::generator<trading::TradeRecord>
  read_trades(stock::Symbol symbol) = 0;
  virtual std::string read_refresh_token(std::string_view service_name) = 0;
  virtual std::optional<std::chrono::system_clock::time_point>
  get_last_notified_at(std::string_view service_name) = 0;
  virtual void update_last_notified_at(std::string_view service_name) = 0;
};

#define DATABASE_TEST(FIXTURE_CLASS)                                           \
  TEST_F(FIXTURE_CLASS, CanSaveCandles) {                                      \
    EXPECT_NO_THROW(save_candle(stock::NVDA, Candle::default_instance()));     \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, SavedCandlesAreReadable) {                             \
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
          ::testing::AllOf(                                                    \
              ::testing::Property("open", &Candle::open, candle.open()),       \
              ::testing::Property("close", &Candle::close, candle.close()),    \
              ::testing::Property("high", &Candle::high, candle.high()),       \
              ::testing::Property("low", &Candle::low, candle.low()),          \
              ::testing::Property("volume", &Candle::volume, candle.volume()), \
              ::testing::Property(                                             \
                  "opened_at",                                                 \
                  &Candle::opened_at,                                          \
                  ::testing::Property(                                         \
                      "seconds",                                               \
                      &::google::protobuf::Timestamp::seconds,                 \
                      candle.opened_at().seconds())),                          \
              ::testing::Property(                                             \
                  "duration",                                                  \
                  &Candle::duration,                                           \
                  ::testing::Property(                                         \
                      "seconds",                                               \
                      &::google::protobuf::Duration::seconds,                  \
                      candle.duration().seconds()))));                         \
    }                                                                          \
    EXPECT_EQ(count, 1);                                                       \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, CanSaveMarket) {                                       \
    EXPECT_NO_THROW(save_market(Market::default_instance()));                  \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, SavedMarketHistoryIsReadable) {                        \
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
          ::testing::AllOf(                                                    \
              ::testing::Property("bid", &Market::bid, market.bid()),          \
              ::testing::Property(                                             \
                  "bid_lots", &Market::bid_lots, market.bid_lots()),           \
              ::testing::Property("ask", &Market::ask, market.ask()),          \
              ::testing::Property(                                             \
                  "ask_lots", &Market::ask_lots, market.ask_lots()),           \
              ::testing::Property("last", &Market::last, market.last()),       \
              ::testing::Property(                                             \
                  "last_lots", &Market::last_lots, market.last_lots()),        \
              ::testing::Property(                                             \
                  "emitted_at",                                                \
                  &Market::emitted_at,                                         \
                  ::testing::Property(                                         \
                      "seconds",                                               \
                      &::google::protobuf::Timestamp::seconds,                 \
                      market.emitted_at().seconds()))));                       \
    }                                                                          \
    EXPECT_EQ(count, 1);                                                       \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, CanSaveTrade) {                                        \
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
    EXPECT_NO_THROW(save_refresh_token("schwab", "token"));                    \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, SavedRefreshTokenIsReadable) {                         \
    save_refresh_token("schwab", "my_secret_token");                           \
    EXPECT_EQ(read_refresh_token("schwab"), "my_secret_token");                \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, ReadingMissingRefreshTokenReturnsEmpty) {              \
    EXPECT_EQ(read_refresh_token("missing_service"), "");                      \
  }                                                                            \
  TEST_F(FIXTURE_CLASS, CanRecordAndReadNotificationTime) {                    \
    save_refresh_token("schwab", "token");                                     \
    EXPECT_FALSE(get_last_notified_at("schwab").has_value());                  \
    update_last_notified_at("schwab");                                         \
    auto last_notified = get_last_notified_at("schwab");                       \
    ASSERT_TRUE(last_notified.has_value());                                    \
    EXPECT_LT(                                                                 \
        std::chrono::system_clock::now() - *last_notified,                     \
        std::chrono::seconds(10));                                             \
  }

} // namespace howling
