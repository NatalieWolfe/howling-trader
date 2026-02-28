#include "services/db/postgres_database.h"

#include <array>
#include <bit>
#include <chrono>
#include <deque>
#include <endian.h>
#include <exception>
#include <format>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/util/time_util.h"
#include "libpq/libpq-fe.h"
#include "services/db/crypto.h"
#include "services/db/schema/schema.h"
#include "time/conversion.h"

namespace howling {
namespace {

using ::std::chrono::duration_cast;
using ::std::chrono::microseconds;
using ::std::chrono::system_clock;

constexpr std::string_view PREPARED_PREFIX = "prepared_";
constexpr int BINARY_FORMAT = 1;
constexpr microseconds PG_EPOCH{946684800000000}; // 2000-01-01.

struct bytes {
  std::string value;
};

void check_pgconn_err(PGconn& conn, std::source_location = {}) {
  int code = PQstatus(&conn);
  if (code != CONNECTION_OK) {
    throw std::runtime_error(
        std::format(
            "Postgres connection error ({}) {}", code, PQerrorMessage(&conn)));
  }
}

void inline_check_command_result(
    PGresult* res, PGconn& conn, std::source_location = {}) {
  int code = PQresultStatus(res);
  if (code != PGRES_COMMAND_OK) {
    std::string message = PQerrorMessage(&conn);
    PQclear(res);
    throw std::runtime_error(
        std::format("Postgres result error ({}) {}", code, message));
  }
  PQclear(res);
}

void check_not_null(PGresult* res, int row, int col) {
  if (PQgetisnull(res, row, col)) {
    throw std::runtime_error("Unexpected NULL value in DB");
  }
}

template <typename T>
unsigned int pg_type_id();

template <>
unsigned int pg_type_id<bool>() {
  return 16;
}

template <>
unsigned int pg_type_id<bytes>() {
  return 17; // BYTEA
}

template <>
unsigned int pg_type_id<int64_t>() {
  return 20;
}

/*
template <>
unsigned int pg_type_id<int16_t>() {
  return 21;
}
*/

template <>
unsigned int pg_type_id<int32_t>() {
  return 23;
}

template <>
unsigned int pg_type_id<std::string_view>() {
  return 25; // Text
}

template <>
unsigned int pg_type_id<double>() {
  return 701;
}

template <>
unsigned int pg_type_id<system_clock::time_point>() {
  return 1114;
}

class query {
private:
  struct already_prepared_t {};
  static constexpr already_prepared_t already_prepared{};

public:
  template <typename... Args>
  static std::unique_ptr<query>
  prepare(PGconn& conn, const std::string& query_str) {
    std::array<unsigned int, sizeof...(Args)> type_ids{pg_type_id<Args>()...};
    std::string name = _to_prepared_name(query_str);
    inline_check_command_result(
        PQprepare(
            &conn,
            /*stmtName=*/name.c_str(),
            query_str.c_str(),
            /*nParams=*/type_ids.size(),
            /*paramTypes=*/type_ids.data()),
        conn);
    return std::make_unique<query>(already_prepared, &conn, std::move(name));
  }

  // For prepared queries.
  query(already_prepared_t, PGconn* conn, std::string name)
      : _conn{conn}, _name{std::move(name)} {}

  // Fallback for ad-hoc queries
  query(PGconn* conn, const std::string& query_str)
      : _conn{conn}, _name{_to_prepared_name(query_str)} {
    inline_check_command_result(
        PQprepare(
            _conn,
            _name.c_str(),
            query_str.c_str(),
            /*nParams=*/0,
            /*paramTypes=*/nullptr),
        *_conn);
  }

  ~query() {
    if (_res) PQclear(_res);
  }

  void execute() {
    if (_res) PQclear(_res);
    _res = PQexecPrepared(
        _conn,
        _name.c_str(),
        /*nParams=*/static_cast<int>(_param_values.size()),
        _param_values.data(),
        _param_lengths.data(),
        _param_formats.data(),
        /*resultFormat=*/BINARY_FORMAT);

    int code = PQresultStatus(_res);
    if (code != PGRES_COMMAND_OK && code != PGRES_TUPLES_OK) {
      inline_check_command_result(_res, *_conn);
    }
    _current_row = -1;
    _total_rows = PQntuples(_res);
  }

  bool step() {
    if (!_res) execute();
    return ++_current_row < _total_rows;
  }

  template <typename... Args>
  void bind_all(Args&&... args) {
    if (_res) {
      PQclear(_res);
      _res = nullptr;
    }
    _param_buffers.clear();
    _param_values.clear();
    _param_lengths.clear();
    _param_formats.clear();
    if constexpr (sizeof...(Args) > 0) {
      _param_values.reserve(sizeof...(Args));
      _param_lengths.reserve(sizeof...(Args));
      _param_formats.reserve(sizeof...(Args));
    }
    (_bind_arg(args), ...);
  }

  template <typename... Outputs>
  void read_all(Outputs&... outs) {
    if (_current_row < 0 || _current_row >= _total_rows) {
      throw std::out_of_range("Current row is out of range.");
    }
    int col = 0;
    (this->_read_column(_current_row, col++, outs), ...);
  }

private:
  static std::string _to_prepared_name(std::string_view query_str) {
    return std::string(PREPARED_PREFIX) +
        std::to_string(std::hash<std::string_view>{}(query_str));
  }

  void _store_param(const void* data, size_t len) {
    std::string& buffer =
        _param_buffers.emplace_back(reinterpret_cast<const char*>(data), len);
    _param_values.push_back(buffer.data());
    _param_lengths.push_back(static_cast<int>(len));
    _param_formats.push_back(BINARY_FORMAT);
  }

  void _bind_arg(int32_t val) {
    val = htobe32(val);
    _store_param(&val, sizeof(val));
  }

  void _bind_arg(int64_t val) {
    val = htobe64(val);
    _store_param(&val, sizeof(val));
  }

  void _bind_arg(double val) {
    uint64_t int_val = htobe64(std::bit_cast<uint64_t>(val));
    _store_param(&int_val, sizeof(int_val));
  }

  void _bind_arg(bool val) {
    val = val ? 1 : 0;
    _store_param(&val, 1);
  }

  void _bind_arg(std::string_view val) { _store_param(val.data(), val.size()); }

  void _bind_arg(const bytes& b) {
    _store_param(b.value.data(), b.value.size());
  }

  void _bind_arg(system_clock::time_point val) {
    microseconds duration =
        duration_cast<microseconds>(val.time_since_epoch() - PG_EPOCH);
    _bind_arg(duration.count());
  }

  template <typename T>
  void _bind_arg(const std::optional<T>& val) {
    if (val) {
      _bind_arg(*val);
    } else {
      _param_values.push_back(nullptr);
      _param_lengths.push_back(0);
      _param_formats.push_back(BINARY_FORMAT);
    }
  }

  void _read_column(int row, int col, std::string_view& out) {
    check_not_null(_res, row, col);
    out = std::string_view{
        PQgetvalue(_res, /*tup_num=*/row, /*field_num=*/col),
        static_cast<size_t>(
            PQgetlength(_res, /*tup_num=*/row, /*field_num=*/col))};
  }

  void _read_column(int row, int col, std::string& out) {
    std::string_view out_view;
    _read_column(row, col, out_view);
    out = std::string{out_view};
  }

  void _read_column(int row, int col, int32_t& out) {
    check_not_null(_res, row, col);
    int32_t net_val;
    memcpy(&net_val, PQgetvalue(_res, row, col), sizeof(net_val));
    out = be32toh(net_val);
  }

  void _read_column(int row, int col, int64_t& out) {
    check_not_null(_res, row, col);
    int64_t net_val;
    memcpy(&net_val, PQgetvalue(_res, row, col), sizeof(net_val));
    // Postgres sends int64 in network byte order
    out = be64toh(net_val);
  }

  void _read_column(int row, int col, double& out) {
    check_not_null(_res, row, col);
    // Postgres sends float8 as IEEE 754 binary, usually big-endian.
    uint64_t net_int;
    memcpy(&net_int, PQgetvalue(_res, row, col), sizeof(net_int));
    out = std::bit_cast<double>(be64toh(net_int));
  }

  void _read_column(int row, int col, bool& out) {
    check_not_null(_res, row, col);
    out = *PQgetvalue(_res, row, col) != 0;
  }

  void _read_column(int row, int col, system_clock::time_point& time) {
    int64_t wire_val;
    _read_column(row, col, wire_val);
    time = system_clock::time_point{PG_EPOCH + microseconds{wire_val}};
  }

  template <typename T>
  void _read_column(int row, int col, std::optional<T>& out) {
    if (PQgetisnull(_res, row, col)) {
      out = std::nullopt;
      return;
    }
    _read_column(row, col, out.emplace());
  }

  PGconn* _conn;
  const std::string _name;
  PGresult* _res = nullptr;
  int _current_row = -1;
  int _total_rows = 0;
  std::deque<std::string> _param_buffers;
  std::vector<const char*> _param_values;
  std::vector<int> _param_lengths;
  std::vector<int> _param_formats;
};

void execute(PGconn& db, const std::string& query_str) {
  inline_check_command_result(PQexec(/*conn=*/&db, query_str.c_str()), db);
}

template <typename... Outputs>
void execute_read(query& q, Outputs&&... outs) {
  if (!q.step()) throw std::runtime_error("No row returned from query.");
  q.read_all(std::forward<Outputs>(outs)...);
  if (q.step()) {
    throw std::runtime_error(
        "More than one row of data returned, but only one expected.");
  }
}

template <typename... Outputs>
void execute_read(PGconn& conn, std::string query_str, Outputs&&... outputs) {
  query q{&conn, query_str};
  execute_read(q, std::forward<Outputs>(outputs)...);
}

void full_schema_install(PGconn& conn) {
  LOG(INFO) << "Performing full DB installation.";
  for (std::string_view statement : db_internal::get_full_schema()) {
    execute(conn, std::string{statement});
  }
}

void upgrade(PGconn& conn) {
  LOG(INFO) << "Checking for howling_version table existence.";
  bool has_version_table = false;
  execute_read(
      conn,
      R"sql(
        SELECT EXISTS (
          SELECT FROM information_schema.tables
          WHERE table_name = 'howling_version'
        )
      )sql",
      has_version_table);
  if (!has_version_table) {
    full_schema_install(conn);
    return;
  }

  LOG(INFO) << "Checking schema version.";
  int version;
  std::optional<int64_t> updater_id;
  std::optional<system_clock::time_point> update_started_at;
  execute_read(
      conn,
      "SELECT v, updater_id, update_started_at FROM howling_version",
      version,
      updater_id,
      update_started_at);
  LOG(INFO) << "Found schema version " << version;

  if (version != db_internal::get_schema_version()) {
    LOG(INFO) << "Upgrading schema from version " << version << " to "
              << db_internal::get_schema_version();
    for (std::string_view statement : db_internal::get_schema_update(version)) {
      execute(conn, std::string{statement});
    }
  }
}

} // namespace

// MARK: Postgres Database

struct postgres_database::implementation {
  PGconn* _conn;
  std::map<std::string, std::unique_ptr<query>> _prepared_queries;
};

postgres_database::postgres_database(postgres_options options)
    : _implementation{std::make_unique<implementation>()} {
  _implementation->_conn = PQsetdbLogin(
      options.host.data(),
      options.port.data(),
      /*options=*/nullptr,
      /*tty=*/nullptr,
      options.dbname.data(),
      options.user.data(),
      options.password.data());

  try {
    check_pgconn_err(*_implementation->_conn);
    upgrade(*_implementation->_conn);

    // Prepare Candle INSERT query
    _implementation->_prepared_queries.emplace(
        "candle_insert",
        query::prepare<
            int,
            double,
            double,
            double,
            double,
            int64_t,
            system_clock::time_point,
            int64_t>(
            *_implementation->_conn,
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

    // Prepare Market INSERT query
    _implementation->_prepared_queries.emplace(
        "market_insert",
        query::prepare<
            int,
            double,
            int64_t,
            double,
            int64_t,
            double,
            int64_t,
            system_clock::time_point>(
            *_implementation->_conn,
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

    // Prepare Candle SELECT query
    _implementation->_prepared_queries.emplace(
        "candle_select", query::prepare<int>(*_implementation->_conn, R"sql(
          SELECT open, close, high, low, volume, opened_at, duration_us
          FROM candles
          WHERE symbol = $1
          ORDER BY opened_at ASC)sql"));

    // Prepare Market SELECT query
    _implementation->_prepared_queries.emplace(
        "market_select", query::prepare<int>(*_implementation->_conn, R"sql(
          SELECT bid, bid_lots, ask, ask_lots, last, last_lots, emitted_at
          FROM market
          WHERE symbol = $1
          ORDER BY emitted_at ASC)sql"));

    // Prepare Trade SELECT query
    _implementation->_prepared_queries.emplace(
        "trade_select", query::prepare<int>(*_implementation->_conn, R"sql(
          SELECT executed_at, action, price, quantity, confidence, dry_run
          FROM trades
          WHERE symbol = $1
          ORDER BY executed_at DESC)sql"));

    // Prepare Trade INSERT query
    _implementation->_prepared_queries.emplace(
        "trade_insert",
        query::prepare<
            int,
            system_clock::time_point,
            int,
            double,
            int64_t,
            double,
            bool>(
            *_implementation->_conn,
            R"sql(
              INSERT INTO trades (
                symbol, executed_at, action, price, quantity, confidence, dry_run
              ) VALUES ($1, $2, $3, $4, $5, $6, $7))sql"));

    // Prepare Refresh Token INSERT query
    _implementation->_prepared_queries.emplace(
        "refresh_token_insert",
        query::prepare<std::string_view, bytes>(
            *_implementation->_conn,
            R"sql(
              INSERT INTO auth_tokens (
                service_name, refresh_token, updated_at
              ) VALUES ($1, $2, CURRENT_TIMESTAMP)
              ON CONFLICT (service_name) DO UPDATE SET
                refresh_token = EXCLUDED.refresh_token,
                updated_at = CURRENT_TIMESTAMP)sql"));

    // Prepare Refresh Token SELECT query
    _implementation->_prepared_queries.emplace(
        "refresh_token_select",
        query::prepare<std::string_view>(
            *_implementation->_conn,
            R"sql(
              SELECT refresh_token
              FROM auth_tokens
              WHERE service_name = $1)sql"));

    // Prepare Last Notification Time SELECT query
    _implementation->_prepared_queries.emplace(
        "get_last_notified_at",
        query::prepare<std::string_view>(
            *_implementation->_conn,
            R"sql(
              SELECT last_notified_at
              FROM auth_tokens
              WHERE service_name = $1)sql"));

    // Prepare Notification Sent UPDATE query
    _implementation->_prepared_queries.emplace(
        "update_last_notified_at",
        query::prepare<std::string_view>(
            *_implementation->_conn,
            R"sql(
              INSERT INTO auth_tokens (
                service_name, refresh_token, last_notified_at, updated_at
              ) VALUES ($1, '', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
              ON CONFLICT (service_name) DO UPDATE SET
                last_notified_at = CURRENT_TIMESTAMP,
                updated_at = CURRENT_TIMESTAMP)sql"));
  } catch (...) {
    if (_implementation->_conn) {
      PQfinish(_implementation->_conn);
      _implementation->_conn = nullptr;
    }
    std::rethrow_exception(std::current_exception());
  }
}

postgres_database::~postgres_database() {
  if (_implementation && _implementation->_conn) {
    PQfinish(_implementation->_conn);
  }
}

std::future<void>
postgres_database::save(stock::Symbol symbol, const Candle& candle) {
  std::promise<void> p;
  try {
    query& q = *_implementation->_prepared_queries.at("candle_insert");
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
    query& q = *_implementation->_prepared_queries.at("market_insert");
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
    query& q = *_implementation->_prepared_queries.at("trade_insert");
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
    query& q = *_implementation->_prepared_queries.at("refresh_token_insert");
    q.bind_all(service_name, bytes{encrypt_token(token)});
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::generator<Candle> postgres_database::read_candles(stock::Symbol symbol) {
  query& q = *_implementation->_prepared_queries.at("candle_select");
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
  query& q = *_implementation->_prepared_queries.at("market_select");
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
  query& q = *_implementation->_prepared_queries.at("trade_select");
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

std::future<std::string>
postgres_database::read_refresh_token(std::string_view service_name) {
  std::promise<std::string> p;
  try {
    query& q = *_implementation->_prepared_queries.at("refresh_token_select");
    q.bind_all(service_name);
    if (!q.step()) {
      p.set_value("");
    } else {
      std::string encrypted_token;
      q.read_all(encrypted_token);
      p.set_value(decrypt_token(encrypted_token));
    }
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<std::optional<system_clock::time_point>>
postgres_database::get_last_notified_at(std::string_view service_name) {
  std::promise<std::optional<system_clock::time_point>> p;
  try {
    query& q = *_implementation->_prepared_queries.at("get_last_notified_at");
    q.bind_all(service_name);
    if (!q.step()) {
      p.set_value(std::nullopt);
    } else {
      std::optional<system_clock::time_point> last_notified;
      q.read_all(last_notified);
      p.set_value(last_notified);
    }
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<void>
postgres_database::update_last_notified_at(std::string_view service_name) {
  std::promise<void> p;
  try {
    query& q =
        *_implementation->_prepared_queries.at("update_last_notified_at");
    q.bind_all(service_name);
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

} // namespace howling
