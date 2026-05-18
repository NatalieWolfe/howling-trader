#include "api/alpaca.h"

#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "boost/beast.hpp"
#include "boost/url.hpp"
#include "containers/vector.h"
#include "data/candle.pb.h"
#include "data/stock.pb.h"
#include "google/protobuf/util/time_util.h"
#include "net/connect.h"
#include "net/url.h"
#include "json/json.h"

ABSL_FLAG(std::string, alpaca_api_key_id, "", "API Key Id for Alpaca");
ABSL_FLAG(std::string, alpaca_api_key_secret, "", "API secret for Alpaca");
ABSL_FLAG(
    std::string,
    alpaca_api_host,
    "data.alpaca.markets",
    "Hostname for the Alpaca API");

namespace howling::alpaca {
namespace {

namespace beast = ::boost::beast;
namespace urls = ::boost::urls;

using ::google::protobuf::util::TimeUtil;

using http_response = beast::http::response<beast::http::dynamic_body>;

void check_alpaca_flags() {
  if (absl::GetFlag(FLAGS_alpaca_api_host).empty()) {
    throw std::runtime_error("--alpaca_api_host flag is required.");
  }
  if (absl::GetFlag(FLAGS_alpaca_api_key_id).empty()) {
    throw std::runtime_error("--alpaca_api_key_id flag is required.");
  }
  if (absl::GetFlag(FLAGS_alpaca_api_key_secret).empty()) {
    throw std::runtime_error("--alpaca_api_key_secret flag is required.");
  }
}

std::string format_time(std::chrono::system_clock::time_point time) {
  using namespace ::std::chrono;
  return std::format("{:%FT%T}Z", floor<milliseconds>(time));
}

net::url
make_url(stock::Symbol symbol, const get_stock_bars_parameters& params) {
  urls::url url;
  url.set_scheme("https");
  url.set_host(absl::GetFlag(FLAGS_alpaca_api_host));
  url.set_path("/v2/stocks/bars");
  url.params().append({"symbols", stock::Symbol_Name(symbol)});
  url.params().append({"timeframe", params.timeframe});
  url.params().append({"limit", std::to_string(params.limit)});
  url.params().append({"sort", params.sort});
  url.params().append({"adjustment", params.adjustment});
  url.params().append({"feed", params.feed});
  url.params().append({"currency", params.currency});

  if (params.start) url.params().append({"start", format_time(*params.start)});
  if (params.end) url.params().append({"end", format_time(*params.end)});
  if (params.page_token) {
    url.params().append({"page_token", *params.page_token});
  }

  return {
      .service = url.scheme(),
      .host = url.host(),
      .target = absl::StrCat(url.path(), "?", url.query())};
}

http_response send_request(
    const std::unique_ptr<net::connection>& conn, const net::url& url) {
  LOG(INFO) << "GET " << url.target;

  // TODO: Set custom user agent.

  beast::http::request<beast::http::string_body> req{
      beast::http::verb::get,
      url.target,
      /*http_version=*/11};
  req.set(beast::http::field::host, url.host);
  req.set(beast::http::field::accept, "application/json");
  req.set("APCA-API-KEY-ID", absl::GetFlag(FLAGS_alpaca_api_key_id));
  req.set("APCA-API-SECRET-KEY", absl::GetFlag(FLAGS_alpaca_api_key_secret));

  beast::http::write(conn->stream(), req);
  beast::flat_buffer buffer;
  http_response res;
  beast::http::read(conn->stream(), buffer, res);

  LOG(INFO) << res.result_int() << " " << res.reason() << " : response("
            << res.body().size() << " bytes)";

  if (res.result_int() != 200) {
    throw std::runtime_error(
        absl::StrCat(
            "Bad response from Alpaca API server: ",
            res.result_int(),
            " ",
            std::string_view{res.reason()}));
  }

  return res;
}

Candle to_candle(const Json::Value& val) {
  Candle candle;
  candle.set_open(val["o"].asDouble());
  candle.set_close(val["c"].asDouble());
  candle.set_high(val["h"].asDouble());
  candle.set_low(val["l"].asDouble());
  candle.set_volume(val["v"].asInt64());
  if (!TimeUtil::FromString(val["t"].asString(), candle.mutable_opened_at())) {
    throw std::runtime_error(
        absl::StrCat(
            "Invalid timestamp format: ",
            Json::writeString(Json::StreamWriterBuilder{}, val["t"]),
            "."));
  }
  // TODO: Build duration from params.timeframe instead of assuming.
  *candle.mutable_duration() = TimeUtil::SecondsToDuration(60);
  return candle;
}

} // namespace

vector<Candle>
get_stock_bars(stock::Symbol symbol, get_stock_bars_parameters params) {
  check_alpaca_flags();

  vector<Candle> candles;
  net::url url = make_url(symbol, params);
  std::unique_ptr<net::connection> conn = net::make_connection(url);
  do {
    http_response res = send_request(conn, url);
    std::stringstream body_stream{beast::buffers_to_string(res.body().data())};

    Json::Value root;
    body_stream >> root;

    for (const Json::Value& val : root["bars"][stock::Symbol_Name(symbol)]) {
      candles.push_back(to_candle(val));
    }

    const Json::Value* next_page_token = root.find("next_page_token");
    bool has_next_page = next_page_token && next_page_token->isString();
    LOG(INFO) << "Loaded " << candles.size() << " results. "
              << (has_next_page ? "Fetching" : "Not fetching")
              << " another page.";

    if (has_next_page) {
      std::string page_token_buffer = next_page_token->asString();
      params.page_token = page_token_buffer;
      url = make_url(symbol, params);
    } else {
      params.page_token = std::nullopt;
    }
  } while (params.page_token);

  return candles;
}

} // namespace howling::alpaca
