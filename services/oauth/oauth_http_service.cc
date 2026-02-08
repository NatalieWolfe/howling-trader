#include "services/oauth/oauth_http_service.h"

#include <chrono>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "boost/asio/basic_waitable_timer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/strand.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/ostream.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http/dynamic_body.hpp"
#include "boost/beast/http/field.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/read.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/beast/http/verb.hpp"
#include "boost/beast/http/write.hpp"
#include "boost/url/url_view.hpp"

namespace howling {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;
namespace urls = ::boost::urls;

using http_response =
    ::boost::beast::http::response<::boost::beast::http::dynamic_body>;
using http_request =
    ::boost::beast::http::request<::boost::beast::http::dynamic_body>;

class oauth_http_connection :
    public std::enable_shared_from_this<oauth_http_connection> {
public:
  explicit oauth_http_connection(asio::ip::tcp::socket socket)
      : _stream{std::move(socket)} {}

  void start() {
    _read_request();
    _schedule_deadline();
  }

private:
  /** Enqueues an asynchronous read from the tcp stream. */
  void _read_request() {
    auto self = shared_from_this();
    http::async_read(
        _stream, _buffer, _request, [self](beast::error_code ec, size_t) {
          if (!ec) self->_process_request();
        });
  }

  /**
   * @brief Dispatches the request to the HTTP verb handler and writes the
   * respones to the wire.
   */
  void _process_request() {
    _response.version(_request.version());
    _response.keep_alive(false);

    try {
      switch (_request.method()) {
        case http::verb::get:
          _process_get();
          break;

        default:
          _response.result(http::status::bad_request);
          _response.set(http::field::content_type, "text/plain");
          beast::ostream(_response.body())
              << "Invalid request method '" << _request.method_string() << "'";
          break;
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "Exception in _process_request: " << e.what();
      _set_internal_error(e.what());
    } catch (...) {
      LOG(ERROR) << "Unknown exception in _process_request";
      _set_internal_error("unknown exception");
    }

    if (_response.result() == http::status::unknown) {
      LOG(ERROR) << "No response set.";
      _set_internal_error("unfinished response");
    }

    _write_response();
  }

  void _process_get() {
    urls::url_view url_view{_request.target()};
    if (url_view.path() != "/callback") {
      _response.result(http::status::not_found);
      _response.set(http::field::content_type, "text/plain");
      beast::ostream(_response.body()) << "File not found.\n";
      return;
    }

    auto code_itr = url_view.params().find("code");
    if (code_itr == url_view.params().end() || (*code_itr).value.empty()) {
      _response.result(http::status::bad_request);
      _response.set(http::field::content_type, "text/plain");
      beast::ostream(_response.body()) << "Missing code.\n";
      return;
    }

    LOG(INFO) << "Received OAuth code";

    // TODO: Exchange code for tokens and store in DB.

    _response.result(http::status::ok);
    _response.set(http::field::content_type, "text/plain");
    beast::ostream(_response.body())
        << "Authentication successful. You may now close this window.\n";
  }

  void _write_response() {
    auto self = shared_from_this();
    _response.set(
        http::field::content_length, std::to_string(_response.body().size()));

    http::async_write(_stream, _response, [self](beast::error_code ec, size_t) {
      self->_stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
      self->_deadline.cancel();
    });
  }

  void _schedule_deadline() {
    auto self = shared_from_this();
    _deadline.async_wait([self](beast::error_code ec) {
      if (!ec) self->_stream.socket().close(ec);
    });
  }

  void _set_internal_error(std::string_view message = "") {
    _response.result(http::status::internal_server_error);
    _response.body().clear();
    auto&& stream = beast::ostream(_response.body());
    stream << "Internal server error";
    if (!message.empty()) {
      stream << ": " << message << "\n";
    } else {
      stream << ".\n";
    }
  }

  beast::tcp_stream _stream;
  // TODO: Record incoming request sizes and monitor in grafana. Determine if a
  // larger buffer size is needed.
  beast::flat_buffer _buffer{8192};
  http_request _request;
  http_response _response;
  asio::basic_waitable_timer<std::chrono::steady_clock> _deadline{
      _stream.get_executor(), std::chrono::seconds(60)};
};

struct oauth_http_service::implementation {
  asio::io_context& ioc;
  asio::ip::tcp::acceptor acceptor;

  implementation(asio::io_context& ioc, unsigned short port)
      : ioc(ioc), acceptor(ioc, {asio::ip::tcp::v4(), port}) {}

  void do_accept() {
    acceptor.async_accept(
        asio::make_strand(ioc),
        [this](beast::error_code ec, asio::ip::tcp::socket socket) {
          if (!ec) {
            std::make_shared<oauth_http_connection>(std::move(socket))->start();
          }
          do_accept();
        });
  }
};

oauth_http_service::oauth_http_service(
    asio::io_context& ioc, unsigned short port)
    : _implementation(std::make_unique<implementation>(ioc, port)) {}

// Defined out-of-line due to incomplete `implementation` struct in header.
oauth_http_service::~oauth_http_service() = default;

void oauth_http_service::start() {
  _implementation->do_accept();
}

} // namespace howling
