#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "containers/vector.h"
#include "data/account.pb.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "net/connect.h"
#include "json/json.h"

namespace howling::schwab {

struct get_history_parameters {
  std::string_view period_type = "day";
  int period = 1;
  std::string_view frequency_type = "minute";
  int frequency = 1;
  std::optional<std::chrono::system_clock::time_point> start_date;
  std::optional<std::chrono::system_clock::time_point> end_date;
  bool need_extended_hours_data = true;
  bool need_previous_close = false;
};

/** Fetches the chart history for the given symbol as a candle series. */
vector<Candle>
get_history(stock::Symbol symbol, const get_history_parameters& params);

std::vector<Account> get_accounts();
std::vector<stock::Position> get_account_positions(std::string_view account_id);

class stream {
public:
  using chart_callback_type = std::function<void(stock::Symbol, Candle)>;
  using market_callback_type = std::function<void(stock::Symbol, Market)>;

  stream();
  ~stream();

  void start();
  void stop();

  void add_symbol(stock::Symbol symbol);

  void on_chart(chart_callback_type cb);
  void on_market(market_callback_type cb);

  bool is_running() const { return _running; }

private:
  using command_callback_type = std::function<void(const Json::Value&)>;
  struct command_parameters {
    std::string_view service;
    std::string_view command;
    Json::Value parameters;
  };
  Json::Value _make_command(command_parameters command);
  void
  _send_command(command_parameters command, command_callback_type cb = nullptr);
  Json::Value _read_message();
  void _process_message();

  void _login();

  int _request_counter = 0;
  std::atomic_bool _running = false;
  std::atomic_bool _stopping = false;
  std::unique_ptr<net::websocket> _conn;
  std::string _customer_id;
  std::string _correlation_id;
  std::unordered_map<int, command_callback_type> _command_cbs;
  command_callback_type _data_cb;
  command_callback_type _market_cb;
};

} // namespace howling::schwab
