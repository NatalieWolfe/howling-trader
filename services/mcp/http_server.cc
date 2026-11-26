#include "services/mcp/http_server.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "boost/asio/dispatch.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/strand.hpp"
#include "boost/asio/thread_pool.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http/error.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/parser.hpp"
#include "boost/beast/http/read.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/http/verb.hpp"
#include "boost/beast/http/write.hpp"
#include "strings/json.h"

namespace howling::mcp {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;

using http_response = http::response<http::string_body>;
using http_request = http::request<http::string_body>;

std::string to_lower_ascii(std::string_view input) {
  std::string output;
  output.reserve(input.size());
  for (unsigned char character : input) {
    output.push_back(static_cast<char>(std::tolower(character)));
  }
  return output;
}

std::string_view trim_ascii(std::string_view input) {
  while (!input.empty() &&
         std::isspace(static_cast<unsigned char>(input.front()))) {
    input.remove_prefix(1);
  }
  while (!input.empty() &&
         std::isspace(static_cast<unsigned char>(input.back()))) {
    input.remove_suffix(1);
  }
  return input;
}

bool is_valid_port_string(std::string_view port_text) {
  if (port_text.empty() || port_text.size() > 5) return false;
  int port_value = 0;
  for (char character : port_text) {
    if (character < '0' || character > '9') return false;
    port_value = (port_value * 10) + (character - '0');
  }
  return port_value >= 1 && port_value <= 65535;
}

// Parses an HTTP quality factor (`qvalue`) in [0.0, 1.0]. Returns 0.0 for
// malformed quality values so invalid `q` parameters never grant acceptance.
double parse_quality_value(std::string_view value) {
  value = trim_ascii(value);
  if (value.empty()) return 0.0;

  if (value == "1") return 1.0;
  if (value.starts_with("1.")) {
    for (char character : value.substr(2)) {
      if (character != '0') return 0.0;
    }
    return 1.0;
  }

  if (value == "0") return 0.0;
  if (value.starts_with("0.")) {
    const std::string_view fraction = value.substr(2);
    double result = 0.0;
    double scale = 0.1;
    for (char character : fraction) {
      if (character < '0' || character > '9') return 0.0;
      result += static_cast<double>(character - '0') * scale;
      scale *= 0.1;
    }
    return result;
  }

  return 0.0;
}

double extract_quality_factor(std::string_view parameters) {
  std::size_t position = 0;
  while (position <= parameters.size()) {
    const std::size_t semicolon = parameters.find(';', position);
    const std::string_view parameter = semicolon == std::string_view::npos
        ? parameters.substr(position)
        : parameters.substr(position, semicolon - position);

    const std::size_t equals = parameter.find('=');
    if (equals != std::string_view::npos) {
      const std::string key =
          to_lower_ascii(trim_ascii(parameter.substr(0, equals)));
      if (key == "q") {
        return parse_quality_value(parameter.substr(equals + 1));
      }
    }

    if (semicolon == std::string_view::npos) break;
    position = semicolon + 1;
  }
  return 1.0;
}

void update_max_quality(std::optional<double>& target, double candidate) {
  if (!target.has_value() || candidate > *target) target = candidate;
}

// Evaluates all `Accept` headers according to RFC 9110 §12.5.1 for
// `application/json`, honoring media-range specificity and `q=0` rejections.
bool accepts_json_response(const http_request& request) {
  auto [accept_begin, accept_end] = request.equal_range(http::field::accept);
  bool has_non_empty_accept = false;
  std::optional<double> exact_quality;
  std::optional<double> type_wildcard_quality;
  std::optional<double> any_wildcard_quality;

  for (auto itr = accept_begin; itr != accept_end; ++itr) {
    const std::string_view header_value = trim_ascii(itr->value());
    if (header_value.empty()) continue;
    has_non_empty_accept = true;

    std::size_t position = 0;
    while (position <= header_value.size()) {
      const std::size_t comma = header_value.find(',', position);
      const std::string_view part = comma == std::string_view::npos
          ? header_value.substr(position)
          : header_value.substr(position, comma - position);

      const std::size_t semicolon = part.find(';');
      const std::string media_range = to_lower_ascii(trim_ascii(
          semicolon == std::string_view::npos ? part
                                              : part.substr(0, semicolon)));
      const double quality = semicolon == std::string_view::npos
          ? 1.0
          : extract_quality_factor(part.substr(semicolon + 1));

      if (media_range == "application/json") {
        update_max_quality(exact_quality, quality);
      } else if (media_range == "application/*") {
        update_max_quality(type_wildcard_quality, quality);
      } else if (media_range == "*/*") {
        update_max_quality(any_wildcard_quality, quality);
      }

      if (comma == std::string_view::npos) break;
      position = comma + 1;
    }
  }

  if (!has_non_empty_accept) return true;
  if (exact_quality.has_value()) return *exact_quality > 0.0;
  if (type_wildcard_quality.has_value()) return *type_wildcard_quality > 0.0;
  if (any_wildcard_quality.has_value()) return *any_wildcard_quality > 0.0;
  return false;
}

std::string_view extract_request_path(std::string_view target) {
  const std::size_t query_position = target.find('?');
  return query_position == std::string_view::npos
      ? target
      : target.substr(0, query_position);
}

http_response make_json_response(
    http::status status,
    unsigned int version,
    bool keep_alive,
    std::string body) {
  http_response response{status, version};
  response.keep_alive(keep_alive);
  response.set(http::field::content_type, "application/json");
  response.body() = std::move(body);
  response.prepare_payload();
  return response;
}

http_response make_empty_response(
    http::status status, unsigned int version, bool keep_alive) {
  http_response response{status, version};
  response.keep_alive(keep_alive);
  response.prepare_payload();
  return response;
}

} // namespace

bool is_allowed_loopback_origin(std::string_view origin) {
  if (origin.empty()) return false;

  const std::size_t scheme_end = origin.find("://");
  if (scheme_end == std::string_view::npos) return false;

  const std::string scheme = to_lower_ascii(origin.substr(0, scheme_end));
  if (scheme != "http" && scheme != "https") return false;

  const std::string_view authority = origin.substr(scheme_end + 3);
  if (authority.empty()) return false;

  // Reject any path, query, fragment, or userinfo components.
  if (authority.find_first_of("/?#@\\") != std::string_view::npos) {
    return false;
  }

  std::string_view host;
  std::string_view port;
  if (authority.front() == '[') {
    const std::size_t close_bracket = authority.find(']');
    if (close_bracket == std::string_view::npos) return false;
    host = authority.substr(0, close_bracket + 1);
    const std::string_view remainder = authority.substr(close_bracket + 1);
    if (!remainder.empty()) {
      if (remainder.front() != ':') return false;
      port = remainder.substr(1);
      if (!is_valid_port_string(port)) return false;
    }
  } else {
    const std::size_t colon = authority.find(':');
    if (colon == std::string_view::npos) {
      host = authority;
    } else {
      host = authority.substr(0, colon);
      port = authority.substr(colon + 1);
      if (!is_valid_port_string(port)) return false;
    }
  }

  const std::string lower_host = to_lower_ascii(host);
  return lower_host == "localhost" || lower_host == "127.0.0.1" ||
      lower_host == "[::1]";
}

struct mcp_http_server::implementation :
    public std::enable_shared_from_this<mcp_http_server::implementation> {
  class http_session;

  asio::io_context& io_context;
  asio::ip::tcp::acceptor acceptor;
  const mcp_dispatcher& dispatcher;
  asio::thread_pool worker_pool;
  std::size_t max_body_bytes;
  std::chrono::seconds request_timeout;
  unsigned short bound_port;
  std::atomic<bool> started{false};
  std::mutex state_mutex;
  bool stopped = false;
  std::unordered_map<http_session*, std::weak_ptr<http_session>>
      active_sessions;
  std::once_flag join_once;

  implementation(
      asio::io_context& io_context,
      const mcp_dispatcher& dispatcher,
      const http_server_options& options)
      : io_context(io_context), acceptor(asio::make_strand(io_context)),
        dispatcher(dispatcher), worker_pool(options.worker_threads),
        max_body_bytes(options.max_body_bytes),
        request_timeout(options.request_timeout), bound_port(0) {
    const asio::ip::address address =
        asio::ip::make_address(options.bind_address);
    const asio::ip::tcp::endpoint endpoint(address, options.port);

    acceptor.open(endpoint.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(asio::socket_base::max_listen_connections);
    bound_port = acceptor.local_endpoint().port();
  }

  bool is_stopped() {
    std::lock_guard<std::mutex> lock(state_mutex);
    return stopped;
  }

  void unregister_session(http_session* session) {
    std::lock_guard<std::mutex> lock(state_mutex);
    active_sessions.erase(session);
  }

  void do_accept();
  void stop();
};

class mcp_http_server::implementation::http_session :
    public std::enable_shared_from_this<http_session> {
public:
  http_session(
      asio::ip::tcp::socket socket,
      std::shared_ptr<mcp_http_server::implementation> server)
      : _stream(std::move(socket)), _server(std::move(server)) {}

  ~http_session() { _server->unregister_session(this); }

  void start() {
    std::shared_ptr<http_session> self = shared_from_this();
    asio::dispatch(_stream.get_executor(), [self]() { self->_read_request(); });
  }

  void close() {
    std::shared_ptr<http_session> self = shared_from_this();
    asio::post(_stream.get_executor(), [self]() { self->_force_close(); });
  }

private:
  void _read_request() {
    if (_closed) return;

    _parser.emplace();
    _parser->body_limit(_server->max_body_bytes);
    _stream.expires_after(_server->request_timeout);

    std::shared_ptr<http_session> self = shared_from_this();
    http::async_read(
        _stream,
        _buffer,
        *_parser,
        [self](beast::error_code error_code, std::size_t) {
          self->_on_read(error_code);
        });
  }

  void _on_read(beast::error_code error_code) {
    if (_closed) return;

    if (error_code == http::error::end_of_stream) {
      _close_gracefully();
      return;
    }
    if (error_code == http::error::body_limit) {
      _write_response(make_json_response(
          http::status::payload_too_large,
          11,
          false,
          R"({"error":"Request body too large"})"));
      return;
    }
    if (error_code) {
      if (error_code != asio::error::operation_aborted &&
          error_code != beast::error::timeout &&
          error_code != asio::error::connection_reset) {
        LOG(WARNING) << "MCP HTTP read error: " << error_code.message();
      }
      _force_close();
      return;
    }

    http_request request = _parser->release();
    _handle_request(std::move(request));
  }

  void _handle_request(http_request request) {
    const unsigned int version = request.version();
    const bool keep_alive = request.keep_alive();
    const std::string_view path = extract_request_path(request.target());

    if (path == "/status") {
      if (request.method() != http::verb::get) {
        http_response response = make_json_response(
            http::status::method_not_allowed,
            version,
            keep_alive,
            R"({"error":"Method not allowed"})");
        response.set(http::field::allow, "GET");
        _write_response(std::move(response));
        return;
      }
      // Handle health checks directly on the I/O strand without touching the
      // worker thread pool.
      _write_response(make_json_response(
          http::status::ok, version, keep_alive, R"({"status":"ok"})"));
      return;
    }

    if (path == "/mcp") {
      // GET (SSE stream) and DELETE (session teardown) intentionally return 405
      // on this stateless MCP server.
      if (request.method() != http::verb::post) {
        http_response response = make_json_response(
            http::status::method_not_allowed,
            version,
            keep_alive,
            R"({"error":"Method not allowed"})");
        response.set(http::field::allow, "POST");
        _write_response(std::move(response));
        return;
      }

      auto [origin_begin, origin_end] =
          request.equal_range(http::field::origin);
      if (origin_begin != origin_end) {
        auto next_origin = origin_begin;
        ++next_origin;
        if (next_origin != origin_end ||
            !is_allowed_loopback_origin(origin_begin->value())) {
          _write_response(make_json_response(
              http::status::forbidden,
              version,
              keep_alive,
              R"({"error":"Forbidden origin"})"));
          return;
        }
      }

      if (!accepts_json_response(request)) {
        _write_response(make_json_response(
            http::status::not_acceptable,
            version,
            keep_alive,
            R"({"error":"Not acceptable"})"));
        return;
      }

      // Hold `state_mutex` while checking `stopped` and posting to the worker
      // pool so `stop()` cannot join the pool while new work is being enqueued.
      std::shared_ptr<http_session> self = shared_from_this();
      std::string body = std::move(request.body());
      bool dispatched = false;
      {
        std::lock_guard<std::mutex> lock(_server->state_mutex);
        if (!_server->stopped) {
          dispatched = true;
          // Offload JSON-RPC dispatch to the worker thread pool so slow
          // handlers never block network I/O or /status probes.
          asio::post(
              _server->worker_pool,
              [self, body = std::move(body), version, keep_alive]() {
                http_response response;
                try {
                  const dispatch_result result =
                      self->_server->dispatcher.dispatch(body);
                  response = !result.has_response()
                      ? make_empty_response(
                            http::status::accepted, version, keep_alive)
                      : make_json_response(
                            http::status::ok,
                            version,
                            keep_alive,
                            to_string(*result.response));
                } catch (const std::exception& error) {
                  LOG(ERROR) << "MCP HTTP dispatch error: " << error.what();
                  response = make_json_response(
                      http::status::internal_server_error,
                      version,
                      keep_alive,
                      R"({"error":"Internal server error"})");
                } catch (...) {
                  LOG(ERROR)
                      << "MCP HTTP dispatch failed with unknown exception.";
                  response = make_json_response(
                      http::status::internal_server_error,
                      version,
                      keep_alive,
                      R"({"error":"Internal server error"})");
                }

                asio::post(
                    self->_stream.get_executor(),
                    [self, response = std::move(response)]() mutable {
                      self->_write_response(std::move(response));
                    });
              });
        }
      }
      if (!dispatched) {
        _write_response(make_json_response(
            http::status::service_unavailable,
            version,
            false,
            R"({"error":"Server shutting down"})"));
      }
      return;
    }

    _write_response(make_json_response(
        http::status::not_found,
        version,
        keep_alive,
        R"({"error":"Not found"})"));
  }

  void _write_response(http_response response) {
    if (_closed) return;

    _response = std::move(response);
    _stream.expires_after(_server->request_timeout);

    std::shared_ptr<http_session> self = shared_from_this();
    const bool keep_alive = _response.keep_alive();
    http::async_write(
        _stream,
        _response,
        [self, keep_alive](beast::error_code error_code, std::size_t) {
          self->_on_write(error_code, keep_alive);
        });
  }

  void _on_write(beast::error_code error_code, bool keep_alive) {
    if (_closed) return;

    if (error_code) {
      if (error_code != asio::error::operation_aborted &&
          error_code != beast::error::timeout &&
          error_code != asio::error::broken_pipe &&
          error_code != asio::error::connection_reset) {
        LOG(WARNING) << "MCP HTTP write error: " << error_code.message();
      }
      _force_close();
      return;
    }

    if (!keep_alive) {
      _close_gracefully();
      return;
    }

    _read_request();
  }

  void _close_gracefully() {
    if (_closed) return;
    _closed = true;
    beast::error_code ignore_error;
    _stream.socket().shutdown(
        asio::ip::tcp::socket::shutdown_send, ignore_error);
    _stream.close();
  }

  void _force_close() {
    if (_closed) return;
    _closed = true;
    beast::error_code ignore_error;
    _stream.socket().shutdown(
        asio::ip::tcp::socket::shutdown_both, ignore_error);
    _stream.close();
  }

  beast::tcp_stream _stream;
  std::shared_ptr<mcp_http_server::implementation> _server;
  beast::flat_buffer _buffer;
  std::optional<http::request_parser<http::string_body>> _parser;
  http_response _response;
  bool _closed = false;
};

void mcp_http_server::implementation::do_accept() {
  if (is_stopped() || !acceptor.is_open()) return;

  std::shared_ptr<implementation> self = shared_from_this();
  acceptor.async_accept(
      asio::make_strand(io_context),
      [self](beast::error_code error_code, asio::ip::tcp::socket socket) {
        if (error_code == asio::error::operation_aborted ||
            !self->acceptor.is_open()) {
          return;
        }

        if (!error_code) {
          auto new_session =
              std::make_shared<http_session>(std::move(socket), self);
          bool registered = false;
          {
            std::lock_guard<std::mutex> lock(self->state_mutex);
            if (!self->stopped) {
              self->active_sessions.emplace(new_session.get(), new_session);
              registered = true;
            }
          }
          if (!registered) return;
          new_session->start();
        } else {
          LOG(WARNING) << "MCP HTTP accept error: " << error_code.message();
        }

        if (!self->is_stopped() && self->acceptor.is_open()) self->do_accept();
      });
}

void mcp_http_server::implementation::stop() {
  std::vector<std::shared_ptr<http_session>> sessions_to_close;
  bool initiate_shutdown = false;
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!stopped) {
      stopped = true;
      initiate_shutdown = true;
      sessions_to_close.reserve(active_sessions.size());
      for (const auto& [raw_session, weak_session] : active_sessions) {
        if (std::shared_ptr<http_session> active_session =
                weak_session.lock()) {
          sessions_to_close.push_back(std::move(active_session));
        }
      }
      active_sessions.clear();
    }
  }

  if (initiate_shutdown) {
    std::shared_ptr<implementation> self = shared_from_this();
    asio::post(acceptor.get_executor(), [self]() {
      beast::error_code ignore_error;
      self->acceptor.close(ignore_error);
    });
    for (const std::shared_ptr<http_session>& active_session :
         sessions_to_close) {
      active_session->close();
    }
  }

  // Block all concurrent callers until every worker thread has finished.
  std::call_once(join_once, [this]() { worker_pool.join(); });
}

mcp_http_server::mcp_http_server(
    asio::io_context& io_context,
    const mcp_dispatcher& dispatcher,
    http_server_options options) {
  if (options.worker_threads == 0) {
    throw std::invalid_argument("Worker thread count must be positive.");
  }
  if (options.max_body_bytes == 0) {
    throw std::invalid_argument("Max body bytes must be positive.");
  }
  if (options.request_timeout <= std::chrono::seconds::zero()) {
    throw std::invalid_argument("Request timeout must be positive.");
  }
  _implementation =
      std::make_shared<implementation>(io_context, dispatcher, options);
}

mcp_http_server::~mcp_http_server() {
  stop();
}

void mcp_http_server::start() {
  if (_implementation->started.exchange(true, std::memory_order_acq_rel)) {
    return;
  }
  std::shared_ptr<implementation> self = _implementation;
  asio::dispatch(
      self->acceptor.get_executor(), [self]() { self->do_accept(); });
}

void mcp_http_server::stop() {
  _implementation->stop();
}

unsigned short mcp_http_server::bound_port() const {
  return _implementation->bound_port;
}

} // namespace howling::mcp
