#include "services/db/postgres_database.h"

#include <chrono>
#include <exception>
#include <format>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "data/trade.pb.h"
#include "google/protobuf/util/time_util.h"
#include "libpq/libpq-fe.h"
#include "services/db/environment.h"
#include "services/db/pg/connection.h"
#include "services/db/pg/query.h"
#include "services/db/pg/types.h"
#include "services/db/schema/auth_token.h"
#include "services/db/schema/schema.h"
#include "time/conversion.h"

namespace howling {
namespace {

using ::google::protobuf::EnumDescriptor;
using ::google::protobuf::EnumValueDescriptor;
using ::std::chrono::duration_cast;
using ::std::chrono::microseconds;
using ::std::chrono::system_clock;

void full_schema_install(PGconn& conn) {
  LOG(INFO) << "Performing full DB installation.";
  for (std::string_view statement : db_internal::get_full_schema()) {
    pg::execute(conn, statement);
  }
}

void sync_proto_enums(PGconn& conn) {
  LOG(INFO) << "Syncing proto enums to database...";
  const EnumDescriptor* symbol_desc = howling::stock::Symbol_descriptor();
  auto insert_symbol =
      pg::query::prepare<int32_t, std::string_view>(conn, R"sql(
    INSERT INTO enum_symbols (id, name)
    VALUES ($1, $2)
    ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name
  )sql");
  for (int i = 0; i < symbol_desc->value_count(); ++i) {
    const EnumValueDescriptor* val = symbol_desc->value(i);
    insert_symbol->bind_all(val->number(), val->name());
    insert_symbol->execute();
  }

  const EnumDescriptor* action_desc = howling::trading::Action_descriptor();
  auto insert_action =
      pg::query::prepare<int32_t, std::string_view>(conn, R"sql(
    INSERT INTO enum_actions (id, name)
    VALUES ($1, $2)
    ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name
  )sql");
  for (int i = 0; i < action_desc->value_count(); ++i) {
    const EnumValueDescriptor* val = action_desc->value(i);
    insert_action->bind_all(val->number(), val->name());
    insert_action->execute();
  }
}

int get_schema_version(PGconn& conn) {
  LOG(INFO) << "Checking for howling_version table existence.";
  bool has_version_table = false;
  pg::execute_read(
      conn,
      R"sql(
        SELECT EXISTS (
          SELECT FROM information_schema.tables
          WHERE table_name = 'howling_version'
        )
      )sql",
      has_version_table);
  if (!has_version_table) return -1;

  LOG(INFO) << "Checking schema version.";
  int32_t version = 0;
  std::optional<int64_t> updater_id;
  std::optional<system_clock::time_point> update_started_at;
  pg::execute_read(
      conn,
      "SELECT v, updater_id, update_started_at FROM howling_version",
      version,
      updater_id,
      update_started_at);
  LOG(INFO) << "Found schema version " << version;
  return version;
}

} // namespace

// MARK: Postgres Database

struct postgres_database::implementation {
  pg::unique_conn conn;
  security_client* security;
  std::map<std::string, std::unique_ptr<pg::query>> prepared_queries;
  std::string dbname;
};

postgres_database::postgres_database(
    security_client& security, postgres_options options)
    : _implementation{std::make_unique<implementation>()} {
  _implementation->security = &security;
  _implementation->dbname = options.dbname;
  _implementation->conn = pg::connect(options);
}

postgres_database::~postgres_database() = default;

bool postgres_database::is_healthy() const {
  if (!_implementation || !_implementation->conn) return false;
  return PQstatus(_implementation->conn.get()) == CONNECTION_OK;
}

std::future<void>
postgres_database::upgrade_schema(std::string_view app_db_user) {
  std::promise<void> p;
  try {
    int version = get_schema_version(*_implementation->conn);
    int expected_version = db_internal::get_schema_version();
    if (version <= 0) {
      full_schema_install(*_implementation->conn);
    } else if (version != expected_version) {
      LOG(INFO) << "Upgrading schema from version " << version << " to "
                << expected_version;
      for (std::string_view statement :
           db_internal::get_schema_update(version)) {
        pg::execute(*_implementation->conn, statement);
      }
    }

    sync_proto_enums(*_implementation->conn);

    if (!app_db_user.empty()) {
      std::string escaped_user =
          pg::escape_identifier(*_implementation->conn, app_db_user);
      pg::execute(
          *_implementation->conn,
          std::format(
              R"sql(GRANT CONNECT ON DATABASE {} TO {})sql",
              pg::escape_identifier(
                  *_implementation->conn, _implementation->dbname),
              escaped_user));
      pg::execute(
          *_implementation->conn,
          std::format(
              R"sql(GRANT USAGE ON SCHEMA public TO {})sql", escaped_user));
      pg::execute(
          *_implementation->conn,
          std::format(
              R"sql(
                GRANT SELECT, INSERT, UPDATE, DELETE
                ON ALL TABLES
                IN SCHEMA public
                TO {})sql",
              escaped_user));
      pg::execute(
          *_implementation->conn,
          std::format(
              R"sql(
                GRANT USAGE, SELECT
                ON ALL SEQUENCES
                IN SCHEMA public
                TO {})sql",
              escaped_user));
    }

    _prepare_queries().get();
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<void> postgres_database::check_schema_version() {
  std::promise<void> p;
  try {
    int db_version = get_schema_version(*_implementation->conn);
    int expected_version = db_internal::get_schema_version();
    if (db_version != expected_version) {
      throw std::runtime_error(
          std::format(
              "Expected schema version {}, found {}",
              expected_version,
              db_version));
    }

    _prepare_queries().get();
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<void> postgres_database::_prepare_queries() {
  std::promise<void> p;
  try {
    _implementation->prepared_queries.emplace(
        "candle_insert",
        pg::query::prepare<
            int32_t,
            double,
            double,
            double,
            double,
            int64_t,
            system_clock::time_point,
            int64_t>(
            *_implementation->conn,
            R"sql(
              INSERT INTO candles (
                symbol, open, close, high, low, volume, opened_at, duration_us
              ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
              ON CONFLICT (symbol, opened_at) DO UPDATE SET
                open = EXCLUDED.open,
                close = EXCLUDED.close,
                high = EXCLUDED.high,
                low = EXCLUDED.low,
                volume = EXCLUDED.volume,
                duration_us = EXCLUDED.duration_us)sql"));

    _implementation->prepared_queries.emplace(
        "market_insert",
        pg::query::prepare<
            int32_t,
            double,
            int64_t,
            double,
            int64_t,
            double,
            int64_t,
            system_clock::time_point>(
            *_implementation->conn,
            R"sql(
              INSERT INTO market (
                symbol, bid, bid_lots, ask, ask_lots, last, last_lots, emitted_at
              ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
              ON CONFLICT (symbol, emitted_at) DO UPDATE SET
                bid = EXCLUDED.bid,
                bid_lots = EXCLUDED.bid_lots,
                ask = EXCLUDED.ask,
                ask_lots = EXCLUDED.ask_lots,
                last = EXCLUDED.last,
                last_lots = EXCLUDED.last_lots)sql"));

    _implementation->prepared_queries.emplace(
        "candle_select",
        pg::query::prepare<int32_t>(*_implementation->conn, R"sql(
          SELECT open, close, high, low, volume, opened_at, duration_us
          FROM candles
          WHERE symbol = $1
          ORDER BY opened_at ASC)sql"));

    _implementation->prepared_queries.emplace(
        "market_select",
        pg::query::prepare<int32_t>(*_implementation->conn, R"sql(
          SELECT bid, bid_lots, ask, ask_lots, last, last_lots, emitted_at
          FROM market
          WHERE symbol = $1
          ORDER BY emitted_at ASC)sql"));

    _implementation->prepared_queries.emplace(
        "trade_select",
        pg::query::prepare<int32_t>(*_implementation->conn, R"sql(
          SELECT executed_at, action, price, quantity, confidence, dry_run
          FROM trades
          WHERE symbol = $1
          ORDER BY executed_at DESC)sql"));

    _implementation->prepared_queries.emplace(
        "trade_insert",
        pg::query::prepare<
            int32_t,
            system_clock::time_point,
            int32_t,
            double,
            int64_t,
            double,
            bool>(
            *_implementation->conn,
            R"sql(
              INSERT INTO trades (
                symbol, executed_at, action, price, quantity, confidence, dry_run
              ) VALUES ($1, $2, $3, $4, $5, $6, $7))sql"));

    _implementation->prepared_queries.emplace(
        "save_refresh_token",
        pg::query::prepare<std::string_view, pg::bytes>(
            *_implementation->conn,
            R"sql(
              INSERT INTO auth_tokens (
                service_name, refresh_token, notice_token, updated_at
              ) VALUES ($1, $2, NULL, CURRENT_TIMESTAMP)
              ON CONFLICT (service_name) DO UPDATE SET
                refresh_token = EXCLUDED.refresh_token,
                notice_token = EXCLUDED.notice_token,
                updated_at = EXCLUDED.updated_at)sql"));

    _implementation->prepared_queries.emplace(
        "get_auth_token",
        pg::query::prepare<std::string_view>(
            *_implementation->conn,
            R"sql(
              SELECT
                service_name,
                refresh_token,
                notice_token,
                last_notified_at,
                updated_at,
                expires_at
              FROM auth_tokens
              WHERE service_name = $1)sql"));

    _implementation->prepared_queries.emplace(
        "save_notice_token",
        pg::query::prepare<std::string_view, std::string_view>(
            *_implementation->conn,
            R"sql(
              INSERT INTO auth_tokens (
                service_name,
                refresh_token,
                notice_token,
                last_notified_at,
                updated_at
              ) VALUES ($1, '', $2, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
              ON CONFLICT (service_name) DO UPDATE SET
                notice_token = EXCLUDED.notice_token,
                last_notified_at = EXCLUDED.last_notified_at,
                updated_at = EXCLUDED.updated_at)sql"));

    _implementation->prepared_queries.emplace(
        "candle_and_market_replay",
        pg::query::prepare<
            int32_t,
            system_clock::time_point,
            system_clock::time_point>(
            *_implementation->conn,
            R"sql(
              SELECT
                'candle' AS type, opened_at AS order_by,
                open, close, high, low, volume, opened_at, duration_us,
                NULL AS bid, NULL AS bid_lots, NULL AS ask, NULL AS ask_lots,
                NULL AS last, NULL AS last_lots, NULL AS emitted_at
              FROM candles
              WHERE symbol = $1 AND opened_at >= $2 AND opened_at < $3
              UNION ALL
              SELECT
                'market' AS type, emitted_at AS order_by,
                NULL AS open, NULL AS close, NULL AS high, NULL AS low,
                NULL AS volume, NULL AS opened_at, NULL AS duration_us,
                bid, bid_lots, ask, ask_lots, last, last_lots, emitted_at
              FROM market
              WHERE symbol = $1 AND emitted_at >= $2 AND emitted_at < $3
              ORDER BY order_by ASC)sql"));
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<void>
postgres_database::save(stock::Symbol symbol, const Candle& candle) {
  std::promise<void> p;
  try {
    pg::query& q = *_implementation->prepared_queries.at("candle_insert");
    q.bind_all(
        static_cast<int>(symbol),
        candle.open(),
        candle.close(),
        candle.high(),
        candle.low(),
        candle.volume(),
        to_std_chrono(candle.opened_at()),
        duration_cast<microseconds>(to_std_chrono(candle.duration())).count());
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<void> postgres_database::save(const Market& market) {
  std::promise<void> p;
  try {
    pg::query& q = *_implementation->prepared_queries.at("market_insert");
    q.bind_all(
        static_cast<int>(market.symbol()),
        market.bid(),
        market.bid_lots(),
        market.ask(),
        market.ask_lots(),
        market.last(),
        market.last_lots(),
        to_std_chrono(market.emitted_at()));
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }

  return p.get_future();
}

std::future<void>
postgres_database::save_trade(const trading::TradeRecord& trade) {
  std::promise<void> p;

  try {
    pg::query& q = *_implementation->prepared_queries.at("trade_insert");
    q.bind_all(
        static_cast<int>(trade.symbol()),
        to_std_chrono(trade.executed_at()),
        static_cast<int>(trade.action()),
        trade.price(),
        trade.quantity(),
        trade.confidence(),
        trade.dry_run());
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }

  return p.get_future();
}

std::future<void> postgres_database::save_refresh_token(
    std::string_view service_name, std::string_view token) {
  std::promise<void> p;
  try {
    pg::query& q = *_implementation->prepared_queries.at("save_refresh_token");
    q.bind_all(
        service_name,
        pg::bytes{_implementation->security->encrypt(
            absl::GetFlag(FLAGS_db_encryption_key_name), token)});
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::generator<Candle> postgres_database::read_candles(stock::Symbol symbol) {
  pg::query& q = *_implementation->prepared_queries.at("candle_select");
  q.bind_all(symbol);
  while (q.step()) {
    double open, close, high, low;
    int64_t volume, duration_us;
    system_clock::time_point opened_at;
    q.read_all(open, close, high, low, volume, opened_at, duration_us);
    Candle candle;
    candle.set_open(open);
    candle.set_close(close);
    candle.set_high(high);
    candle.set_low(low);
    candle.set_volume(volume);
    *candle.mutable_opened_at() = to_proto(opened_at);
    *candle.mutable_duration() = to_proto(microseconds(duration_us));
    co_yield std::move(candle);
  }
}

std::generator<Market> postgres_database::read_market(stock::Symbol symbol) {
  pg::query& q = *_implementation->prepared_queries.at("market_select");
  q.bind_all(symbol);
  while (q.step()) {
    double bid, ask, last;
    int64_t bid_lots, ask_lots, last_lots;
    system_clock::time_point emitted_at;
    q.read_all(bid, bid_lots, ask, ask_lots, last, last_lots, emitted_at);
    Market market;
    market.set_symbol(symbol);
    market.set_bid(bid);
    market.set_bid_lots(bid_lots);
    market.set_ask(ask);
    market.set_ask_lots(ask_lots);
    market.set_last(last);
    market.set_last_lots(last_lots);
    *market.mutable_emitted_at() = to_proto(emitted_at);
    co_yield std::move(market);
  }
}

std::generator<trading::TradeRecord>
postgres_database::read_trades(stock::Symbol symbol) {
  pg::query& q = *_implementation->prepared_queries.at("trade_select");
  q.bind_all(symbol);
  while (q.step()) {
    system_clock::time_point executed_at;
    int32_t act_int;
    double price, confidence;
    int64_t quantity;
    bool dry_run;
    q.read_all(executed_at, act_int, price, quantity, confidence, dry_run);
    trading::TradeRecord trade;
    trade.set_symbol(symbol);
    *trade.mutable_executed_at() = to_proto(executed_at);
    trade.set_action(static_cast<trading::Action>(act_int));
    trade.set_price(price);
    trade.set_quantity(quantity);
    trade.set_confidence(confidence);
    trade.set_dry_run(dry_run);
    co_yield std::move(trade);
  }
}

std::generator<std::variant<Candle, Market>> postgres_database::replay(
    stock::Symbol symbol,
    std::chrono::system_clock::time_point start_at,
    std::chrono::system_clock::time_point end_at) {
  pg::query& q =
      *_implementation->prepared_queries.at("candle_and_market_replay");
  q.bind_all(symbol, start_at, end_at);
  while (q.step()) {
    std::string_view type;
    system_clock::time_point order_by;
    std::optional<system_clock::time_point> opened_at, emitted_at;
    std::optional<double> open, close, high, low, bid, ask, last;
    std::optional<int64_t> volume, duration_us, bid_lots, ask_lots, last_lots;
    q.read_all(
        type,
        order_by,
        open,
        close,
        high,
        low,
        volume,
        opened_at,
        duration_us,
        bid,
        bid_lots,
        ask,
        ask_lots,
        last,
        last_lots,
        emitted_at);
    if (type == "candle") {
      if (!open || !close || !high || !low || !volume || !opened_at ||
          !duration_us) {
        throw std::runtime_error("Replay candle row missing required values.");
      }
      Candle candle;
      candle.set_open(*open);
      candle.set_close(*close);
      candle.set_high(*high);
      candle.set_low(*low);
      candle.set_volume(*volume);
      *candle.mutable_opened_at() = to_proto(*opened_at);
      *candle.mutable_duration() = to_proto(microseconds(*duration_us));
      co_yield std::move(candle);
    } else if (type == "market") {
      if (!bid || !bid_lots || !ask || !ask_lots || !last || !last_lots ||
          !emitted_at) {
        throw std::runtime_error("Replay market row missing required values.");
      }
      Market market;
      market.set_symbol(symbol);
      market.set_bid(*bid);
      market.set_bid_lots(*bid_lots);
      market.set_ask(*ask);
      market.set_ask_lots(*ask_lots);
      market.set_last(*last);
      market.set_last_lots(*last_lots);
      *market.mutable_emitted_at() = to_proto(*emitted_at);
      co_yield std::move(market);
    } else {
      throw std::runtime_error(
          std::format("Unexpected type in replay query: {}", type));
    }
  }
}

std::future<std::optional<storage::auth_token>>
postgres_database::get_auth_token(std::string_view service_name) {
  std::promise<std::optional<storage::auth_token>> p;
  try {
    pg::query& q = *_implementation->prepared_queries.at("get_auth_token");
    q.bind_all(service_name);
    if (!q.step()) {
      p.set_value(std::nullopt);
    } else {
      storage::auth_token row;
      q.read_all(
          row.service_name,
          row.refresh_token,
          row.notice_token,
          row.last_notified_at,
          row.updated_at,
          row.expires_at);
      if (!row.refresh_token.empty()) {
        try {
          row.refresh_token = _implementation->security->decrypt(
              absl::GetFlag(FLAGS_db_encryption_key_name), row.refresh_token);
        } catch (const std::exception& e) {
          LOG(WARNING) << "Failed to decrypt refresh token for service \""
                       << service_name << "\": " << e.what()
                       << ". Treating token as expired/invalid.";
          row.refresh_token.clear();
        }
      }
      p.set_value(std::move(row));
    }
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<void> postgres_database::save_notice_token(
    std::string_view service_name, std::string_view notice_token) {
  std::promise<void> p;
  try {
    pg::query& q = *_implementation->prepared_queries.at("save_notice_token");
    q.bind_all(service_name, notice_token);
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

} // namespace howling
