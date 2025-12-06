#include "api/schwab.h"

#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "api/json.h"
#include "api/schwab/connect.h"
#include "api/schwab/oauth.h"
#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "boost/url.hpp"
#include "containers/vector.h"
#include "data/candle.pb.h"
#include "data/market.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/util/time_util.h"
#include "net/connect.h"
#include "net/url.h"
#include "strings/parse.h"
#include "time/conversion.h"
#include "json/json.h"

namespace howling::schwab {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace fs = ::std::filesystem;
namespace urls = ::boost::urls;

using ::google::protobuf::util::TimeUtil;

using http_response =
    ::boost::beast::http::response<::boost::beast::http::dynamic_body>;

enum class stream_code : int { SUCCESS = 0 };

bool operator==(const Json::Value& json, stream_code code) {
  return json.isInt() && json == static_cast<int>(code);
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

Json::Value send_request(
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

  return to_json(beast::buffers_to_string(res.body().data()));
}

std::string format_time(const std::chrono::system_clock::time_point& time) {
  using namespace ::std::chrono;
  return std::to_string(
      duration_cast<milliseconds>(time.time_since_epoch()).count());
}

net::url make_url(stock::Symbol symbol, const get_history_parameters& params) {
  urls::url url;
  url.set_scheme("https");
  url.set_host(get_schwab_host());
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
  net::url user_pref_url = make_net_url("/trader/v1/userPreference");
  std::unique_ptr<net::connection> api_conn =
      net::make_connection(user_pref_url);
  std::string_view bearer_token = get_bearer_token(api_conn);
  Json::Value root = send_request(api_conn, bearer_token, user_pref_url);
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
  std::vector<Candle> candles;
  net::url url = make_url(symbol, params);
  std::unique_ptr<net::connection> conn = net::make_connection(url);
  std::string_view bearer_token = get_bearer_token(conn);

  Json::Value root = send_request(conn, bearer_token, url);
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
