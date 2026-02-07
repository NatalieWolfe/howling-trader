#include "api/schwab/oauth.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "api/json.h"
#include "api/schwab/connect.h"
#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "boost/url.hpp"
#include "howling_tools/runfiles.h"
#include "net/connect.h"
#include "strings/trim.h"
#include "json/json.h"

ABSL_FLAG(std::string, schwab_api_key_id, "", "API key ID for Schwab.");
ABSL_FLAG(std::string, schwab_api_key_secret, "", "API secret for Schwab.");

namespace howling::schwab {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace fs = ::std::filesystem;
namespace urls = ::boost::urls;

using http_response =
    ::boost::beast::http::response<::boost::beast::http::dynamic_body>;
using http_request =
    ::boost::beast::http::request<::boost::beast::http::dynamic_body>;
using ssl_socket = ::boost::asio::ssl::stream<::boost::asio::ip::tcp::socket>;

const fs::path REFRESH_TOKEN_FILENAME{"refresh-token"};
constexpr std::string_view REDIRECT_URL =
    "https://local.wolfe.dev:15986/schwab/oauth-callback";

class oauth_callback_server_connection :
    public std::enable_shared_from_this<oauth_callback_server_connection> {
public:
  template <typename Callback>
  oauth_callback_server_connection(
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
      fs::path{std::getenv("HOME")} / ".cache/howling-trader";
  return cache_path;
}

/** Checks the Schwab API key flags are set. */
void check_schwab_flags() {
  if (absl::GetFlag(FLAGS_schwab_api_key_id).empty()) {
    throw std::runtime_error("--schwab_api_key_id flag is required.");
  }
  if (absl::GetFlag(FLAGS_schwab_api_key_secret).empty()) {
    throw std::runtime_error("--schwab_api_key_secret flag is required.");
  }
}

struct oauth_exchange_params {
  std::optional<std::string_view> code;
  std::optional<std::string_view> refresh_token;
};
Json::Value send_oauth_request(
    const std::unique_ptr<net::connection>& conn,
    oauth_exchange_params params) {
  check_schwab_flags();

  net::url oauth_url = make_net_url("/v1/oauth/token");
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

  // Stash the refresh token to disk if available.
  // TODO: Encrypt the refresh token on disk.
  Json::Value root = to_json(beast::buffers_to_string(res.body().data()));
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
                    std::make_shared<oauth_callback_server_connection>(
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
  url.set_host(get_schwab_host());
  url.set_path("/v1/oauth/authorize");
  url.params().append({"client_id", absl::GetFlag(FLAGS_schwab_api_key_id)});
  url.params().append({"redirect_uri", REDIRECT_URL});
  url.params().append({"response_type", "code"});

  std::string command =
      absl::StrCat("xdg-open \"", std::string_view{url.buffer()}, "\"");
  LOG(INFO) << command;
  if (std::system(command.c_str()) != 0) {
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

} // namespace

std::string_view get_bearer_token(
    const std::unique_ptr<net::connection>& conn, bool clear_cache) {
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
  if (!root) root = execute_oauth(conn);

  // Cache the retrieved token with its expiration. We will refresh the token
  // early to avoid any clock skew.
  cached_token = (*root)["access_token"].asString();
  cache_expiration = steady_clock::now() +
      seconds((*root)["expires_in"].asInt64()) - minutes(1);

  return cached_token;
}

} // namespace howling::schwab
