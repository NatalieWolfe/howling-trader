#include "services/db/sqlite_database.h"

#include <cctype>
#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <limits>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/util/time_util.h"
#include "services/db/crypto.h"
#include "services/db/schema/schema.h"
#include "sqlite3.h"
#include "strings/format.h"
#include "strings/parse.h"
#include "time/conversion.h"

ABSL_FLAG(
    std::string,
    sqlite_db_path,
    "howling.db",
    "Path to the SQLite database file.");

namespace howling {
namespace {

using std::chrono::system_clock;

void check_sqlite_err(
    int code, sqlite3* db = nullptr, std::source_location = {}) {
  // TODO: Figure out what to do with source loc. Possibly log error?
  if (code != SQLITE_OK) {
    std::string_view message;
    if (db) {
      message = sqlite3_errmsg(db);
    } else {
      message = sqlite3_errstr(code);
    }
    throw std::runtime_error(
        absl::StrCat(
            "SQLite error (",
            code,
            ") ",
            message.empty() ? "<unknown>" : message));
  }
}

void check_sqlite_err(int code, sqlite3& db, std::source_location loc = {}) {
  check_sqlite_err(code, &db, std::move(loc));
}

class query {
private:
  struct single_use_t {};

public:
  static constexpr single_use_t single_use = {};

  query(sqlite3& db, std::string_view query_str) : _db{db} {
    check_sqlite_err(
        sqlite3_prepare_v3(
            &_db,
            query_str.data(),
            query_str.size(),
            SQLITE_PREPARE_PERSISTENT,
            &_statement,
            /*tail=*/nullptr),
        _db);
  }

  query(sqlite3& db, single_use_t, std::string_view query_str) : _db{db} {
    check_sqlite_err(
        sqlite3_prepare_v2(
            &_db,
            query_str.data(),
            query_str.size(),
            &_statement,
            /*tail=*/nullptr),
        _db);
  }

  ~query() {
    if (_statement) sqlite3_finalize(_statement);
  }

  template <typename T>
  void bind(int index, T&& val) {
    _bind(index, std::forward<T>(val));
  }

  template <typename... Args>
  void bind_all(Args&&... args) {
    int counter = 0;
    (_bind(++counter, std::forward<Args>(args)), ...);
  }

  /**
   * Steps execution of the prepared statement forward.
   *
   * @return True if there is a row of data to be read, otherwise false.
   */
  bool step() {
    int res = sqlite3_step(_statement);
    if (res == SQLITE_BUSY) {
      throw std::runtime_error("Handling query try currently unsupported.");
    } else if (res == SQLITE_DONE) {
      // TODO: Handle query reset logic.
      return false;
    } else if (res == SQLITE_ROW) {
      return true;
    } else if (res == SQLITE_MISUSE) {
      throw std::runtime_error("Implementation error with query.");
    } else {
      check_sqlite_err(res, _db);
      throw std::runtime_error("Unknown sqlite error.");
    }
  }

  template <typename T>
  T read(int index) {
    T val;
    _read_column(index, val);
    return val;
  }

  template <typename... Args>
  void read_all(Args&&... args) {
    int column_count = sqlite3_data_count(_statement);
    if (column_count != sizeof...(Args)) {
      throw std::runtime_error(
          absl::StrCat(
              "Query has ",
              column_count,
              " columns to read, but ",
              sizeof...(Args),
              " columns requested."));
    }
    int counter = 0;
    (_read_column(counter++, std::forward<Args>(args)), ...);
  }

private:
  // MARK: Bind
  void _bind(int index, int32_t n) {
    check_sqlite_err(sqlite3_bind_int(_statement, index, n), _db);
  }

  void _bind(int index, int64_t n) {
    check_sqlite_err(sqlite3_bind_int64(_statement, index, n), _db);
  }

  void _bind(int index, double n) {
    check_sqlite_err(sqlite3_bind_double(_statement, index, n), _db);
  }

  void _bind(int index, std::nullptr_t) {
    check_sqlite_err(sqlite3_bind_null(_statement, index), _db);
  }

  void _bind(int index, std::string_view str) {
    check_sqlite_err(
        sqlite3_bind_text(
            _statement, index, str.data(), str.size(), SQLITE_TRANSIENT),
        _db);
  }

  void _bind(int index, system_clock::time_point time) {
    _bind(index, to_string(time));
  }

  // MARK: Read
  void _read_column(int index, int32_t& n) {
    if (sqlite3_column_type(_statement, index) != SQLITE_INTEGER) {
      throw std::runtime_error(
          absl::StrCat(
              "Attempted to read non-integer column (",
              sqlite3_column_type(_statement, index),
              ") into an integer."));
    }
    n = sqlite3_column_int(_statement, index);
  }

  void _read_column(int index, int64_t& n) {
    if (sqlite3_column_type(_statement, index) != SQLITE_INTEGER) {
      throw std::runtime_error(
          "Attempted to read non-integer column into an integer.");
    }
    n = sqlite3_column_int64(_statement, index);
  }

  void _read_column(int index, double& d) {
    if (sqlite3_column_type(_statement, index) != SQLITE_FLOAT) {
      throw std::runtime_error(
          "Attempted to read non-float column into a float.");
    }
    d = sqlite3_column_double(_statement, index);
  }

  void _read_column(int index, std::string_view& str) {
    if (sqlite3_column_type(_statement, index) != SQLITE_TEXT) {
      throw std::runtime_error(
          "Attempted to read non-text column into a string.");
    }
    int size = sqlite3_column_bytes(_statement, index);
    if (size < 0) {
      throw std::runtime_error(
          absl::StrCat("Invalid size for text column received: ", size));
    }
    if (size == 0) {
      str = "";
      return;
    }
    str = std::string_view{
        reinterpret_cast<const char*>(sqlite3_column_text(_statement, index)),
        static_cast<size_t>(size)};
  }

  void _read_column(int index, std::string& str) {
    std::string_view str_view;
    _read_column(index, str_view);
    str.assign(str_view);
  }

  void _read_column(int index, system_clock::time_point& time) {
    std::string_view time_str;
    _read_column(index, time_str);
    time = parse_timepoint(time_str);
  }

  template <typename T>
  void _read_column(int index, std::optional<T>& optional_val) {
    if (sqlite3_column_type(_statement, index) == SQLITE_NULL) {
      optional_val = std::nullopt;
      return;
    }
    _read_column(index, optional_val.emplace());
  }

  sqlite3& _db;
  sqlite3_stmt* _statement = nullptr;
};

void execute(sqlite3& db, std::string_view query_str) {
  query q{db, query::single_use, query_str};
  if (q.step()) {
    throw std::runtime_error("Data returned from one-off query execution.");
  }
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
void execute_read(
    sqlite3& db, std::string_view query_str, Outputs&&... outputs) {
  query q{db, query::single_use, query_str};
  execute_read(q, std::forward<Outputs>(outputs)...);
}

void full_schema_install(sqlite3& db) {
  LOG(INFO) << "Performing full DB installation.";
  for (std::string_view statement : db_internal::get_full_schema()) {
    execute(db, statement);
  }
}

void upgrade(sqlite3& db) {
  LOG(INFO) << "Checking for howling_version table existence.";
  int has_version_table = 0;
  execute_read(
      db,
      R"sql(
        SELECT (
          SELECT 1
          FROM sqlite_schema WHERE name = 'howling_version'
        ) IS NOT NULL
      )sql",
      has_version_table);
  if (!has_version_table) {
    full_schema_install(db);
    return;
  }

  LOG(INFO) << "Checking schema version.";
  int version;
  std::optional<int64_t> updater_id;
  std::optional<system_clock::time_point> update_started_at;
  execute_read(
      db,
      "SELECT v, updater_id, update_started_at FROM howling_version",
      version,
      updater_id,
      update_started_at);

  if (version != db_internal::get_schema_version()) {
    LOG(INFO) << "Upgrading schema from version " << version << " to "
              << db_internal::get_schema_version();
    for (std::string_view statement : db_internal::get_schema_update(version)) {
      execute(db, statement);
    }
  }
}

} // namespace

// TODO: Configure the sqlite logging system to use absl LOG.

sqlite_database::sqlite_database() {
  try {
    _check(sqlite3_open(absl::GetFlag(FLAGS_sqlite_db_path).c_str(), &_db));
    upgrade(*_db);
  } catch (...) {
    if (_db) {
      sqlite3_close_v2(_db);
      _db = nullptr;
    }
    std::rethrow_exception(std::current_exception());
  }
}

sqlite_database::~sqlite_database() {
  if (_db) sqlite3_close_v2(_db);
}

std::future<void>
sqlite_database::save(stock::Symbol symbol, const Candle& candle) {
  std::promise<void> p;
  using std::chrono::duration_cast;
  using std::chrono::microseconds;
  try {
    // TODO: Save insert query and re-use.
    query q{*_db, query::single_use, R"sql(
    INSERT OR REPLACE INTO candles (
      symbol, open, close, high, low, volume, opened_at, duration_us
    ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8))sql"};
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

std::future<void> sqlite_database::save(const Market& market) {
  std::promise<void> p;
  try {
    // TODO: Save insert query and re-use.
    query q{*_db, query::single_use, R"sql(
    INSERT OR REPLACE INTO market (
      symbol, bid, bid_lots, ask, ask_lots, last, last_lots, emitted_at
    ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8))sql"};
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
sqlite_database::save_trade(const trading::TradeRecord& trade) {
  std::promise<void> p;

  try {
    query q{*_db, query::single_use, R"sql(
      INSERT INTO trades (
        symbol, executed_at, action, price, quantity, confidence, dry_run
      ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7))sql"};
    q.bind_all(
        static_cast<int>(trade.symbol()),
        to_std_chrono(trade.executed_at()),
        static_cast<int>(trade.action()),
        trade.price(),
        trade.quantity(),
        trade.confidence(),
        trade.dry_run() ? 1 : 0);
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }

  return p.get_future();
}

std::future<void> sqlite_database::save_refresh_token(
    std::string_view service_name, std::string_view token) {
  std::promise<void> p;
  try {
    query q{*_db, query::single_use, R"sql(
      INSERT INTO auth_tokens (
        service_name, refresh_token, updated_at
      ) VALUES (?1, ?2, CURRENT_TIMESTAMP)
      ON CONFLICT (service_name) DO UPDATE SET
        refresh_token = excluded.refresh_token,
        updated_at = CURRENT_TIMESTAMP)sql"};
    q.bind_all(service_name, encrypt_token(token));
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::generator<Candle> sqlite_database::read_candles(stock::Symbol symbol) {
  query q{*_db, query::single_use, R"sql(
    SELECT open, close, high, low, volume, opened_at, duration_us
    FROM candles
    WHERE symbol = ?1
    ORDER BY opened_at ASC
  )sql"};
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
    *candle.mutable_duration() =
        to_proto(std::chrono::microseconds(duration_us));
    co_yield std::move(candle);
  }
}

std::generator<Market> sqlite_database::read_market(stock::Symbol symbol) {
  query q{*_db, query::single_use, R"sql(
    SELECT bid, bid_lots, ask, ask_lots, last, last_lots, emitted_at
    FROM market
    WHERE symbol = ?1
    ORDER BY emitted_at ASC
  )sql"};
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
sqlite_database::read_trades(stock::Symbol symbol) {
  query q{*_db, query::single_use, R"sql(
    SELECT executed_at, action, price, quantity, confidence, dry_run
    FROM trades
    WHERE symbol = ?1
    ORDER BY executed_at DESC
  )sql"};
  q.bind_all(symbol);
  while (q.step()) {
    system_clock::time_point executed_at;
    int32_t act_int, dry_run_int;
    double price, confidence;
    int64_t quantity;
    q.read_all(executed_at, act_int, price, quantity, confidence, dry_run_int);
    trading::TradeRecord trade;
    trade.set_symbol(symbol);
    *trade.mutable_executed_at() = to_proto(executed_at);
    trade.set_action(static_cast<trading::Action>(act_int));
    trade.set_price(price);
    trade.set_quantity(quantity);
    trade.set_confidence(confidence);
    trade.set_dry_run(dry_run_int != 0);
    co_yield std::move(trade);
  }
}

std::future<std::string>
sqlite_database::read_refresh_token(std::string_view service_name) {
  std::promise<std::string> p;
  try {
    query q{*_db, query::single_use, R"sql(
      SELECT refresh_token
      FROM auth_tokens
      WHERE service_name = ?1)sql"};
    q.bind_all(service_name);
    if (!q.step()) {
      p.set_value("");
    } else {
      p.set_value(decrypt_token(q.read<std::string>(0)));
    }
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<std::optional<system_clock::time_point>>
sqlite_database::get_last_notified_at(std::string_view service_name) {
  std::promise<std::optional<system_clock::time_point>> p;
  try {
    query q{*_db, query::single_use, R"sql(
      SELECT last_notified_at
      FROM auth_tokens
      WHERE service_name = ?1)sql"};
    q.bind_all(service_name);
    if (!q.step()) {
      p.set_value(std::nullopt);
    } else {
      p.set_value(q.read<std::optional<system_clock::time_point>>(0));
    }
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

std::future<void>
sqlite_database::update_last_notified_at(std::string_view service_name) {
  std::promise<void> p;
  try {
    query q{*_db, query::single_use, R"sql(
      INSERT INTO auth_tokens (
        service_name, refresh_token, last_notified_at, updated_at
      ) VALUES (?1, '', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
      ON CONFLICT (service_name) DO UPDATE SET
        last_notified_at = CURRENT_TIMESTAMP,
        updated_at = CURRENT_TIMESTAMP)sql"};
    q.bind_all(service_name);
    while (q.step());
    p.set_value();
  } catch (...) { p.set_exception(std::current_exception()); }
  return p.get_future();
}

void sqlite_database::_check(int code, std::source_location loc) {
  check_sqlite_err(code, _db, std::move(loc));
}

} // namespace howling
