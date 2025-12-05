#include "api/schwab.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "boost/url.hpp"
#include "containers/vector.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/util/time_util.h"
#include "howling_tools/runfiles.h"
#include "net/connect.h"
#include "net/url.h"
#include "strings/parse.h"
#include "strings/trim.h"
#include "time/conversion.h"
#include "json/json.h"

ABSL_FLAG(std::string, schwab_api_key_id, "", "API key ID for Schwab.");
ABSL_FLAG(std::string, schwab_api_key_secret, "", "API secret for Schwab.");
ABSL_FLAG(
    std::string,
    schwab_api_host,
    "api.schwabapi.com",
    "Hostname for the Schwab API.");

namespace howling::schwab {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace fs = ::std::filesystem;
namespace urls = ::boost::urls;

using ::google::protobuf::util::TimeUtil;

using http_response =
    ::boost::beast::http::response<::boost::beast::http::dynamic_body>;
using http_request =
    ::boost::beast::http::request<::boost::beast::http::dynamic_body>;
using ssl_socket = ::boost::asio::ssl::stream<::boost::asio::ip::tcp::socket>;

constexpr std::string_view REDIRECT_URL =
    "https://local.wolfe.dev:15986/schwab/oauth-callback";
const fs::path REFRESH_TOKEN_FILENAME{"refresh-token"};

enum class stream_code : int { SUCCESS = 0 };

bool operator==(const Json::Value& json, stream_code code) {
  return json.isInt() && json == static_cast<int>(code);
}

class http_server_connection :
    public std::enable_shared_from_this<http_server_connection> {
public:
  template <typename Callback>
  http_server_connection(
      std::shared_ptr<ssl_socket> stream, Callback&& callback)
      : _stream{std::move(stream)},
        _code_callback{std::forward<Callback>(callback)} {}

  /** Reads the incoming request and initiates a deadline timer. */
  void start() {
    _read_request();
    _schedule_deadline();
  }

private:
  /** Read the entire request into the buffer. */
  void _read_request() {
    beast::http::async_read(
        *_stream,
        _buffer,
        _request,
        [self = shared_from_this()](beast::error_code ec, size_t) {
          if (!ec) self->_process_request();
        });
  }

  void _process_request() {
    _response.version(_request.version());
    _response.keep_alive(false);

    switch (_request.method()) {
      case beast::http::verb::get:
        _process_get();
        break;

      default:
        // Only GET supported.
        _response.result(beast::http::status::bad_request);
        _response.set(beast::http::field::content_type, "text/plain");
        beast::ostream(_response.body())
            << "Invalid request-method '" << _request.method_string() << "'";
        break;
    }

    _write_response();
  }

  void _process_get() {
    // Only requests to the callback URI are supported.
    urls::url_view url_view{_request.target()};
    if (url_view.path() != "/schwab/oauth-callback") {
      _response.result(beast::http::status::not_found);
      _response.set(beast::http::field::content_type, "text/plain");
      beast::ostream(_response.body()) << "File not found.\n";
      return;
    }

    // Must contain the authorization code as a URL parameter.
    auto code_itr = url_view.params().find("code");
    if (code_itr == url_view.params().end() || (*code_itr).value.empty()) {
      _response.result(beast::http::status::bad_request);
      _response.set(beast::http::field::content_type, "text/plain");
      beast::ostream(_response.body()) << "Missing code.\n";
      return;
    }

    _response.result(beast::http::status::ok);
    _response.set(beast::http::field::content_type, "text/plain");
    beast::ostream(_response.body()) << "You may now close this window.\n";
    // TODO: Make the window self-closing.

    // Send the authorization code to the callback so the OAuth sequence can
    // continue.
    _stream->get_executor().execute(
        [self = shared_from_this(), code = (*code_itr).value]() {
          self->_code_callback(code);
        });
  }

  void _write_response() {
    _response.set(
        beast::http::field::content_length,
        std::to_string(_response.body().size()));

    beast::http::async_write(
        *_stream,
        _response,
        [self = shared_from_this()](beast::error_code ec, size_t) {
          self->_stream->shutdown(ec);
          self->_deadline.cancel();
        });
  }

  /** Starts a deadline timer running. */
  void _schedule_deadline() {
    _deadline.async_wait([self = shared_from_this()](beast::error_code ec) {
      if (!ec) self->_stream->next_layer().close(ec);
    });
  }

  std::shared_ptr<ssl_socket> _stream;
  beast::flat_buffer _buffer{8192};
  http_request _request;
  http_response _response;
  boost::asio::basic_waitable_timer<std::chrono::steady_clock> _deadline{
      _stream->get_executor(), std::chrono::seconds(60)};
  std::function<void(std::string)> _code_callback;
};

/** Returns the path to the cache folder for the current user. */
fs::path get_user_cache_folder() {
  static const fs::path cache_path =
      fs::path{getenv("HOME")} / ".cache" / "howling-trader";
  return cache_path;
}

/** Checks that all the required flags are set. */
void check_schwab_flags() {
  if (absl::GetFlag(FLAGS_schwab_api_host).empty()) {
    throw std::runtime_error("--schwab_api_host flag is required.");
  }
  if (absl::GetFlag(FLAGS_schwab_api_key_id).empty()) {
    throw std::runtime_error("--schwab_api_key_id flag is required.");
  }
  if (absl::GetFlag(FLAGS_schwab_api_key_secret).empty()) {
    throw std::runtime_error("--schwab_api_key_secret flag is required.");
  }
}

void check_json(
    bool passed, std::source_location loc = std::source_location::current()) {
  if (!passed) {
    throw std::runtime_error(
        absl::StrCat(
            "[",
            loc.file_name(),
            ":",
            loc.line(),
            "] Invalid JSON schema received."));
  }
}

http_response send_request(
    const std::unique_ptr<net::connection>& conn,
    std::string_view bearer_token,
    const net::url& url) {
  LOG(INFO) << "GET " << url.target;

  // TODO: Set custom user agent.

  using http_headers = beast::http::field;
  beast::http::request<beast::http::string_body> req{
      beast::http::verb::get,
      url.target,
      /*http_version=*/11};
  req.set(http_headers::host, url.host);
  req.set(http_headers::accept, "application/json");
  req.set(http_headers::authorization, absl::StrCat("Bearer ", bearer_token));

  beast::http::write(conn->stream(), req);
  beast::flat_buffer buffer;
  http_response res;
  beast::http::read(conn->stream(), buffer, res);

  LOG(INFO) << res.result_int() << " " << res.reason() << " : response("
            << res.body().size() << " bytes)";

  if (res.result_int() != 200) {
    LOG(ERROR) << beast::buffers_to_string(res.body().data());
    throw std::runtime_error(
        absl::StrCat(
            "Bad response from Schwab API server: ",
            res.result_int(),
            " ",
            std::string_view{res.reason()}));
  }

  return res;
}

Json::Value to_json(std::string str) {
  std::stringstream body_stream{std::move(str)};
  Json::Value root;
  body_stream >> root;
  return root;
}

Json::Value to_json(const http_response& res) {
  return to_json(beast::buffers_to_string(res.body().data()));
}

struct oauth_exchange_params {
  std::optional<std::string_view> code;
  std::optional<std::string_view> refresh_token;
};

Json::Value send_oauth_request(
    const std::unique_ptr<net::connection>& conn,
    oauth_exchange_params params) {
  net::url oauth_url{
      .service = "https",
      .host = absl::GetFlag(FLAGS_schwab_api_host),
      .target = "/v1/oauth/token"};
  LOG(INFO) << "POST " << oauth_url.target;

  // Initialize request with authorization headers.
  using http_headers = ::boost::beast::http::field;
  beast::http::request<beast::http::string_body> req{
      beast::http::verb::post,
      oauth_url.target,
      /*http_version=*/11};
  req.set(http_headers::host, oauth_url.host);
  req.set(
      http_headers::authorization,
      absl::StrCat(
          "Basic ",
          absl::Base64Escape(
              absl::StrCat(
                  absl::GetFlag(FLAGS_schwab_api_key_id),
                  ":",
                  absl::GetFlag(FLAGS_schwab_api_key_secret)))));
  req.set(http_headers::accept, "application/json");

  // Construct the request body as URL-encoded parameters.
  urls::url body;
  if (params.code) {
    body.params().append({"grant_type", "authorization_code"});
    body.params().append({"redirect_uri", REDIRECT_URL});
    body.params().append({"code", *params.code});
  } else if (params.refresh_token) {
    body.params().append({"grant_type", "refresh_token"});
    body.params().append({"refresh_token", *params.refresh_token});
  } else {
    throw std::runtime_error(
        "Missing authorization code or refresh token for OAuth request.");
  }
  req.set(http_headers::content_length, std::to_string(body.query().size()));
  req.set(http_headers::content_type, "application/x-www-form-urlencoded");
  req.body() = body.query();

  // Send the request, read the response.
  beast::http::write(conn->stream(), req);
  beast::flat_buffer buffer;
  http_response res;
  beast::http::read(conn->stream(), buffer, res);

  // Throw on error.
  LOG(INFO) << res.result_int() << " " << res.reason();
  if (res.result_int() != 200) {
    LOG(ERROR) << beast::buffers_to_string(res.body().data());
    throw std::runtime_error(
        absl::StrCat(
            "Bad response from Schwab API server: ",
            res.result_int(),
            " ",
            std::string_view{res.reason()}));
  }
  LOG(INFO) << "response(" << res.body().size() << " bytes)";

  // Stash the refresh token to disk if available.
  // TODO: Encrypt the refresh token on disk.
  Json::Value root = to_json(res);
  const Json::Value* next_refresh_token = root.find("refresh_token");
  if (next_refresh_token && next_refresh_token->isString()) {
    fs::create_directories(get_user_cache_folder());
    std::ofstream cached_refresh_file{
        get_user_cache_folder() / REFRESH_TOKEN_FILENAME};
    cached_refresh_file << next_refresh_token->asString() << std::endl;
  }

  return root;
}

/** Attempts to refresh authorization using the token stored to disk. */
std::optional<Json::Value>
refresh_token(const std::unique_ptr<net::connection>& conn) {
  fs::path refresh_token_path =
      get_user_cache_folder() / REFRESH_TOKEN_FILENAME;
  if (!fs::exists(refresh_token_path)) return std::nullopt;

  std::ifstream stream(refresh_token_path);
  std::stringstream data;
  data << stream.rdbuf();
  std::string refresh{trim(data.str())};
  if (!refresh.empty()) {
    LOG(INFO) << "Refreshing token from disk.";
    try {
      Json::Value root = send_oauth_request(conn, {.refresh_token = refresh});
      const Json::Value* bearer_token = root.find("access_token");
      if (bearer_token && bearer_token->isString()) return root;
      throw std::runtime_error("Unexpected response from oauth request.");
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to refresh oauth token: " << e.what();
    } catch (...) {
      LOG(ERROR) << "!!!! UNKNOWN FAILURE REFRESHING OAUTH TOKEN !!!!";
    }
  }

  LOG(ERROR) << "Clearing cached refresh token after failed request.";
  fs::remove(refresh_token_path);
  return std::nullopt;
}

/** Runs a new oauth sequence with the user by opening the browser. */
Json::Value execute_oauth(const std::unique_ptr<net::connection>& conn) {
  // Run an HTTPS server in another thread which will listen for the OAuth
  // callback with the authentication code to be exchanged for the bearer token.
  std::string auth_code;
  std::jthread server_thread{[&]() {
    // localhost:15986
    asio::ip::address address = asio::ip::make_address("0.0.0.0");
    unsigned short port = 15986;

    // Configure the SSL context using the pre-packaged self-signed certs. The
    // browser will reject these certs initially, so it will be necessary to
    // type `thisisunsafe` at least once.
    asio::ssl::context ssl_context{asio::ssl::context::tlsv13};
    ssl_context.set_options(
        asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
        asio::ssl::context::single_dh_use);
    ssl_context.use_certificate_chain_file(
        runfile("howling-trader/net/local.wolfe.dev.crt"));
    ssl_context.use_private_key_file(
        runfile("howling-trader/net/local.wolfe.dev.key"),
        asio::ssl::context::pem);

    // On new connection, run the SSL handshake sequence and then pass the
    // socket over to the `http_connection` class to manage the request.
    asio::io_context io_context{/*concurrency_hint=*/1};
    asio::ip::tcp::acceptor acceptor{io_context, {address, port}};
    std::function<void()> do_accept;
    do_accept = [&]() {
      auto stream = std::make_shared<asio::ssl::stream<asio::ip::tcp::socket>>(
          io_context, ssl_context);
      acceptor.async_accept(
          stream->next_layer(), [&, stream](beast::error_code ec) {
            if (ec) {
              LOG(ERROR)
                  << "Error accepting connections on local loopback service: "
                  << ec.message();
            } else {
              stream->async_handshake(
                  asio::ssl::stream_base::server,
                  [&, stream](beast::error_code ec) {
                    if (ec) {
                      // TODO: Reduce logging when its cert rejected.
                      LOG(ERROR) << "Handshake failed: " << ec.message();
                      return;
                    }
                    std::make_shared<http_server_connection>(
                        stream,
                        [&](std::string code) {
                          auth_code = std::move(code);
                          io_context.stop();
                        })
                        ->start();
                  });
            }

            if (!io_context.stopped()) do_accept();
          });
    };
    do_accept();
    io_context.run();
  }};

  // Now that the server is running, open up the browser to the Schwab auth
  // page.
  urls::url url;
  url.set_scheme("https");
  url.set_host(absl::GetFlag(FLAGS_schwab_api_host));
  url.set_path("/v1/oauth/authorize");
  url.params().append({"client_id", absl::GetFlag(FLAGS_schwab_api_key_id)});
  url.params().append({"redirect_uri", REDIRECT_URL});
  url.params().append({"response_type", "code"});

  std::string command =
      absl::StrCat("xdg-open \"", std::string_view{url.buffer()}, "\"");
  LOG(INFO) << command;
  if (::system(command.c_str()) != 0) {
    throw std::runtime_error("Failed to open browser to Schwab OAuth flow.");
  }

  // Server will shut down once the code is retrieved, so we can use the thread
  // as a semaphore for the auth code being set.
  server_thread.join();
  if (auth_code.empty()) {
    throw std::runtime_error("Failed to retrieve authorization from Schwab.");
  }
  return send_oauth_request(conn, {.code = auth_code});
}

/**
 * Returns a bearer token to use for the API.
 *
 * In order, this method gets the bearer token from:
 *  - In-memory cache of the bearer token,
 *  - Refreshing the token from a disk-cached refresh token, or
 *  - Executing a fresh OAuth login sequence.
 *
 * The latter options will cache the retrieved bearer token in memory upon
 * success so subsequent calls will be quicker.
 */
std::string_view get_bearer_token(
    const std::unique_ptr<net::connection>& conn, bool clear_cache = false) {
  // Check for a cached bearer token in memory.
  using namespace ::std::chrono;
  static std::string cached_token;
  static steady_clock::time_point cache_expiration;
  if (clear_cache || steady_clock::now() > cache_expiration) {
    cached_token.clear();
    cache_expiration = steady_clock::time_point{};
  }
  if (!cached_token.empty()) return cached_token;
  if (!conn) throw std::runtime_error("No cached bearer token available.");

  // Try to refresh the bearer token using a refresh token saved to disk. If
  // refresh fails, runs a new OAuth sequence.
  std::optional<Json::Value> root = refresh_token(conn);
  if (!root) { root = execute_oauth(conn); }

  // Cache the retrieved token with its expiration. We will refresh the token
  // early to avoid any clock skew.
  cached_token = (*root)["access_token"].asString();
  cache_expiration = steady_clock::now() +
      seconds((*root)["expires_in"].asInt64()) - minutes(1);

  return cached_token;
}

std::string format_time(const std::chrono::system_clock::time_point& time) {
  using namespace ::std::chrono;
  return std::to_string(
      duration_cast<milliseconds>(time.time_since_epoch()).count());
}

net::url make_url(stock::Symbol symbol, const get_history_parameters& params) {
  urls::url url;
  url.set_scheme("https");
  url.set_host(absl::GetFlag(FLAGS_schwab_api_host));
  url.set_path("/marketdata/v1/pricehistory");
  url.params().append({"symbol", stock::Symbol_Name(symbol)});
  url.params().append({"periodType", params.period_type});
  url.params().append({"period", std::to_string(params.period)});
  url.params().append({"frequencyType", params.frequency_type});
  url.params().append({"frequency", std::to_string(params.frequency)});

  if (params.start_date) {
    url.params().append({"startDate", format_time(*params.start_date)});
  }
  if (params.end_date) {
    url.params().append({"endDate", format_time(*params.end_date)});
  }
  if (params.need_extended_hours_data) {
    url.params().append({"needExtendedHoursData", "true"});
  }
  if (params.need_previous_close) {
    url.params().append({"needPreviousClose", "true"});
  }

  return {
      .service = url.scheme(),
      .host = url.host(),
      .target = absl::StrCat(url.path(), "?", url.query())};
}

std::string to_string(const Json::Value& root) {
  static const Json::StreamWriterBuilder builder = ([]() {
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    return builder;
  })();
  return Json::writeString(builder, root);
}

Json::Value get_streamer_info() {
  net::url user_pref_url = {
      .service = "https",
      .host = absl::GetFlag(FLAGS_schwab_api_host),
      .target = "/trader/v1/userPreference"};
  std::unique_ptr<net::connection> api_conn =
      net::make_connection(user_pref_url);
  std::string_view bearer_token = get_bearer_token(api_conn);
  Json::Value root =
      to_json(send_request(api_conn, bearer_token, user_pref_url));
  Json::Value streamer_info = Json::nullValue;

  check_json(root.isObject());
  const Json::Value* streamer = root.find("streamerInfo");
  check_json(streamer && streamer->isArray());
  streamer_info = streamer->front();
  check_json(streamer_info.isObject());

  return streamer_info;
}

} // namespace

// MARK: get_history

vector<Candle>
get_history(stock::Symbol symbol, const get_history_parameters& params) {
  check_schwab_flags();

  std::vector<Candle> candles;
  net::url url = make_url(symbol, params);
  std::unique_ptr<net::connection> conn = net::make_connection(url);
  std::string_view bearer_token = get_bearer_token(conn);

  Json::Value root = to_json(send_request(conn, bearer_token, url));
  for (const Json::Value& val : root["candles"]) {
    Candle& candle = candles.emplace_back();
    candle.set_open(val["open"].asDouble());
    candle.set_close(val["close"].asDouble());
    candle.set_high(val["high"].asDouble());
    candle.set_low(val["low"].asDouble());
    candle.set_volume(val["volume"].asInt64());
    // TODO: Include and use correct proto time util methods.
    *candle.mutable_opened_at() =
        TimeUtil::MillisecondsToTimestamp(val["datetime"].asInt64());
    *candle.mutable_duration() = TimeUtil::SecondsToDuration(60);
  }

  return candles;
}

// MARK: stream

stream::stream() {
  check_schwab_flags();
  _data_cb = [](const Json::Value&) {
    LOG(WARNING) << "Dropping data packet. No chart callback registered.";
  };
  _market_cb = [](const Json::Value&) {
    LOG(WARNING) << "Dropping data packet. No market callback registered.";
  };
}

stream::~stream() {
  if (_conn && _running) stop();
}

void stream::start() {
  _stopping = false;
  _login();

  _running = true;
  while (!_stopping) _process_message();
  _running = false;
}

void stream::stop() {
  if (!_conn) {
    throw std::runtime_error("Schwab API stream never started, cannot stop.");
  }
  _stopping = true;

  _send_command(
      {.service = "ADMIN",
       .command = "LOGOUT",
       .parameters = Json::objectValue},
      [this](const Json::Value&) {
        LOG(INFO) << "Closing stream";
        _conn->stream().close(beast::websocket::close_code::normal);
        _conn = nullptr;
      });

  try {
    while (_conn) _process_message();
  } catch (const boost::system::system_error& e) {
    if (e.code() != asio::ssl::error::stream_truncated) {
      LOG(WARNING) << "Unexpected error while shutting down stream: ["
                   << e.code() << "] " << e.what();
    }
  }
}

// MARK: stream data

void stream::add_symbol(stock::Symbol symbol) {
  Json::Value parameters{Json::objectValue};
  parameters["keys"] = stock::Symbol_Name(symbol);
  auto callback = [symbol](const Json::Value& response) {
    const Json::Value* content = response.find("content");
    check_json(content && content->isObject());
    const Json::Value* code = content->find("code");
    check_json(code && code->isInt());
    if (*code != stream_code::SUCCESS) {
      const Json::Value* service = response.find("service");
      const Json::Value* msg = content->find("msg");
      throw std::runtime_error(
          std::format(
              "Failed to add {} ({}) to {} stream: [{}] {}",
              stock::Symbol_Name(symbol),
              static_cast<int>(symbol),
              service ? service->asString() : "<unknown service>",
              code->asInt(),
              msg ? msg->asString() : "unknown"));
    }
  };

  // Chart fields:
  //
  // Field   | Name     | Type   | Description
  // --------|----------|--------|------------------------------------
  // 0 (key) | Symbol   | String | Ticker symbol in upper case
  // 1 (seq) | Sequence | long   | Identifies the candle minute
  // 2       | Open     | double | Opening price for the minute
  // 3       | High     | double | Highest price for the minute
  // 4       | Low      | double | Chart's lowest price for the minute
  // 5       | Close    | double | Closing price for the minute
  // 6       | Volume   | double | Total volume for the minute
  // 7       | Time     | long   | Milliseconds since Epoch
  parameters["fields"] = "0,1,2,3,4,5,6,7";
  _send_command(
      {.service = "CHART_EQUITY", .command = "ADD", .parameters = parameters},
      callback);

  // Levelone fields:
  //
  // Field | Name   | Type   | Description
  // 0     | Symbol | String | Ticker symbol in upper case
  // 1     | Bid $  | double | Current Bid Price
  // 2     | Ask $  | double | Current Ask Price
  // 3     | Last $ | double | Price at which the last trade was matched
  // 4     | Bid #  | int    | Number of shares for bid
  // 5     | Ask #  | int    | Number of shares for ask
  // 9     | Last # | long   | Number of shares traded with last trade
  parameters["fields"] = "0,1,2,3,4,5,9";
  _send_command(
      {.service = "LEVELONE_EQUITIES",
       .command = "ADD",
       .parameters = std::move(parameters)},
      callback);
}

void stream::on_chart(chart_callback_type cb) {
  _data_cb = [cb = std::move(cb)](const Json::Value& data) {
    const Json::Value* content = data.find("content");
    check_json(content && content->isArray());
    for (const Json::Value& json_candle : *content) {
      Candle candle;
      // See add_symbol for key reference table.
      candle.set_open(json_candle.find("2")->asDouble());
      candle.set_close(json_candle.find("5")->asDouble());
      candle.set_high(json_candle.find("3")->asDouble());
      candle.set_low(json_candle.find("4")->asDouble());
      candle.set_volume(
          static_cast<int64_t>(json_candle.find("6")->asDouble()));

      using namespace ::std::chrono;
      system_clock::time_point time{
          milliseconds{json_candle.find("7")->asInt64()}};
      system_clock::time_point opened_at = floor<minutes>(time);
      auto duration = time - opened_at;

      *candle.mutable_opened_at() = to_proto(opened_at);
      *candle.mutable_duration() =
          to_proto(duration == milliseconds(0) ? seconds(60) : duration);

      stock::Symbol symbol;
      if (!stock::Symbol_Parse(json_candle.find("key")->asString(), &symbol)) {
        throw std::runtime_error(
            absl::StrCat(
                "Unknown stock symbol returned: ",
                json_candle.find("key")->asString()));
      }

      cb(symbol, std::move(candle));
    }
  };
}

void stream::on_market(market_callback_type cb) {
  _market_cb = [cb = std::move(cb)](const Json::Value& data) {
    const Json::Value* content = data.find("content");
    const Json::Value* timestamp = data.find("timestamp");
    check_json(content && content->isArray());
    check_json(timestamp && timestamp->isInt64());
    auto time = to_proto(
        std::chrono::system_clock::time_point{
            std::chrono::milliseconds{timestamp->asInt64()}});

    for (const Json::Value& json_market : *content) {
      Market market;

      // See add_symbol for key reference table.
      market.set_bid(json_market.get("1", 0).asDouble());
      market.set_bid_lots(json_market.get("4", 0).asInt64());
      market.set_ask(json_market.get("2", 0).asDouble());
      market.set_ask_lots(json_market.get("5", 0).asInt64());
      market.set_last(json_market.get("3", 0).asDouble());
      market.set_last_lots(json_market.get("9", 0).asDouble());
      *market.mutable_emitted_at() = time;

      stock::Symbol symbol;
      if (!stock::Symbol_Parse(json_market.find("key")->asString(), &symbol)) {
        throw std::runtime_error(
            absl::StrCat(
                "Unknown stock symbol returned: ",
                json_market.find("key")->asString()));
      }
      market.set_symbol(symbol);

      cb(symbol, std::move(market));
    }
  };
}

// MARK: stream commands

Json::Value stream::_make_command(command_parameters command) {
  Json::Value root{Json::objectValue};
  root["requestid"] = std::to_string(_request_counter++);
  root["service"] = std::string{command.service};
  root["command"] = std::string{command.command};
  root["SchwabClientCustomerId"] = _customer_id;
  root["SchwabClientCorrelId"] = _correlation_id;
  root["parameters"] = std::move(command.parameters);
  return root;
}

void stream::_send_command(
    command_parameters command, command_callback_type cb) {
  if (cb) {
    _command_cbs[_request_counter] =
        [this, request_id = _request_counter, cb = std::move(cb)](
            const Json::Value& response) {
          cb(response);
          _command_cbs.erase(request_id);
        };
  }
  std::string command_string = to_string(_make_command(std::move(command)));
  // LOG(INFO) << "[S] " << command_string;
  _conn->stream().write(asio::buffer(std::move(command_string)));
}

// MARK: stream messages

Json::Value stream::_read_message() {
  Json::Value message;
  auto is_heartbeat = [](const Json::Value& message) {
    return message.find("notify") != nullptr;
  };
  do {
    beast::flat_buffer buffer;
    _conn->stream().read(buffer);
    message = to_json(beast::buffers_to_string(buffer.cdata()));
  } while (is_heartbeat(message));
  return message;
}

void stream::_process_message() {
  Json::Value message = _read_message();
  // LOG(INFO) << "[R] " << to_string(message);
  if (message.find("notify") != nullptr) return; // Heartbeat.
  const Json::Value* key = message.find("data");
  if (key != nullptr) {
    check_json(key->isArray());
    for (const Json::Value& datum : *key) {
      const Json::Value* service = datum.find("service");
      check_json(service && service->isString());
      if (*service == "CHART_EQUITY") {
        _data_cb(datum);
      } else if (*service == "LEVELONE_EQUITIES") {
        _market_cb(datum);
      } else {
        LOG(ERROR) << "Unknown data service received: " << service->asString();
      }
    }
    return;
  }
  key = message.find("response");
  check_json(key && key->isArray());
  for (const Json::Value& response : *key) {
    const Json::Value* request_id = response.find("requestid");
    check_json(request_id && request_id->isString());
    auto callback_itr = _command_cbs.find(parse_int(request_id->asString()));
    if (callback_itr != _command_cbs.end()) callback_itr->second(response);
  }
}

// MARK: stream login

void stream::_login() {
  Json::Value streamer_info = get_streamer_info();
  urls::url stream_url{streamer_info["streamerSocketUrl"].asString()};
  _customer_id = streamer_info["schwabClientCustomerId"].asString();
  _correlation_id = streamer_info["schwabClientCorrelId"].asString();

  _conn = net::make_websocket(
      {.service = "443",
       .host = stream_url.host(),
       .target = std::string{stream_url.encoded_target()}});
  _conn->stream().text(true);

  Json::Value parameters{Json::objectValue};
  parameters["Authorization"] = std::string{get_bearer_token(/*conn=*/nullptr)};
  parameters["SchwabClientChannel"] =
      streamer_info["schwabClientChannel"].asString();
  parameters["SchwabClientFunctionId"] =
      streamer_info["schwabClientFunctionId"].asString();
  Json::Value login_response = Json::nullValue;
  _send_command(
      {.service = "ADMIN",
       .command = "LOGIN",
       .parameters = std::move(parameters)},
      [&login_response](const Json::Value& res) { login_response = res; });

  while (login_response.isNull()) _process_message();

  if (!login_response) {
    throw std::runtime_error("Never received login response!");
  }
  check_json(login_response.isObject());
  const Json::Value* service = login_response.find("service");
  const Json::Value* command = login_response.find("command");
  check_json(service && *service == "ADMIN");
  check_json(command && *command == "LOGIN");

  const Json::Value* content = login_response.find("content");
  check_json(content);
  const Json::Value* code = content->find("code");
  check_json(code && code->isInt());
  if (*code != stream_code::SUCCESS) {
    const Json::Value* msg = content->find("msg");
    throw std::runtime_error(
        absl::StrCat(
            "Failed to login to streaming API (",
            code->asInt(),
            "): ",
            msg ? msg->asString() : "unknown"));
  }
}

} // namespace howling::schwab
