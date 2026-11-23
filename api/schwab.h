#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "containers/vector.h"
#include "data/account.pb.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "functional/retry.h"
#include "net/connect.h"
#include "signaling/cancellation.h"
#include "json/json.h"

namespace howling::schwab {

// TODO: Introduce a virtual base interface for this class to enable mocking and
// facilitate unit testing of services that depend on it.
class api_connection {
public:
  api_connection();

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
  std::vector<stock::Position>
  get_account_positions(std::string_view account_id);

  struct order_parameters {
    std::string_view account_id;
    stock::Symbol symbol;
    double price;
    int quantity;

    std::string_view session = "NORMAL";
    std::string_view duration = "DAY";
    std::string_view order_type = "LIMIT";
    std::string_view order_strategy = "SINGLE";
    std::string_view complexity_strategy = "NONE";
  };

  void place_buy(const order_parameters& params);
  void place_sell(const order_parameters& params);

private:
  std::unique_ptr<net::connection> _conn;
};

/**
 * @brief Configuration options for a Schwab WebSocket stream.
 */
struct stream_options {
  net::websocket_options websocket_options{};
  backoff_options reconnect_backoff{
      .initial_delay = std::chrono::seconds{1},
      .maximum_delay = std::chrono::seconds{60},
      .deadline = std::chrono::steady_clock::duration::max(),
      .exponent = 2.0,
  };
};

/**
 * @brief Streams real-time chart and level-one equity updates from Schwab over
 * WebSocket with automatic dead-connection detection and reconnect.
 *
 * Thread safety:
 * - stop() and is_running() are thread-safe and may be called from any thread
 *   at any time.
 * - on_chart() and on_market() must be configured before calling start().
 * - add_symbol() may be called before start() or from the thread running
 *   start() (for example, inside the start() callback).
 * - start() must be invoked on a single thread at a time.
 */
class stream {
public:
  using chart_callback_type = std::function<void(stock::Symbol, Candle)>;
  using market_callback_type = std::function<void(stock::Symbol, Market)>;

  stream();
  explicit stream(stream_options options);
  stream(const stream&) = delete;
  stream& operator=(const stream&) = delete;
  ~stream();

  /**
   * @brief Connects to the Schwab streaming API and processes messages until
   * stop() is called, automatically reconnecting and re-subscribing on errors
   * or read timeouts.
   */
  void start(std::function<void()> callback = nullptr);

  /**
   * @brief Thread-safely signals the stream to stop, cancels any active socket
   * or reconnect wait, and waits for start() to exit when called from another
   * thread.
   */
  void stop();

  /**
   * @brief Subscribes to chart and market updates for the given symbol and
   * retains it for automatic re-subscription after reconnects.
   */
  void add_symbol(stock::Symbol symbol);

  void on_chart(chart_callback_type cb);
  void on_market(market_callback_type cb);

  /**
   * @brief Returns whether the stream is currently connected and actively
   * receiving messages.
   */
  [[nodiscard]] bool is_running() const { return _running.load(); }

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
  void _subscribe_to_symbols(std::span<const stock::Symbol> symbols);
  Json::Value _read_message();
  void _process_message();
  void _login();
  bool _connect_with_backoff(
      const std::function<void()>& callback,
      bool& initial_callback_invoked,
      int& reconnect_attempt);
  void _establish_connection(
      const std::function<void()>& callback,
      bool& initial_callback_invoked,
      int& reconnect_attempt);
  void _run_message_loop(int& reconnect_attempt);
  void _cleanup_connection();
  void _handle_stream_error(const std::exception& err);
  [[nodiscard]] bool _is_stopping() const;

  stream_options _options;
  cancellation _cancellation;
  int _request_counter = 0;
  std::atomic_bool _running = false;
  mutable std::mutex _state_mutex;
  std::condition_variable _stopped_cv;
  bool _stopping = false;
  bool _in_start = false;
  std::thread::id _start_thread_id;
  std::shared_ptr<net::websocket> _conn;
  std::vector<stock::Symbol> _subscribed_symbols;
  std::unordered_set<stock::Symbol> _subscribed_on_current_conn;
  bool _received_healthy_traffic = false;
  std::string _customer_id;
  std::string _correlation_id;
  std::unordered_map<int, command_callback_type> _command_cbs;
  command_callback_type _data_cb;
  command_callback_type _market_cb;
};

} // namespace howling::schwab
