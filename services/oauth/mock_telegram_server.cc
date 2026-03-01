#include "services/oauth/mock_telegram_server.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "environment/runfiles.h"

ABSL_DECLARE_FLAG(std::string, telegram_bot_token);
ABSL_DECLARE_FLAG(std::string, telegram_chat_id);
ABSL_DECLARE_FLAG(std::string, telegram_host);
ABSL_DECLARE_FLAG(uint16_t, telegram_port);

namespace howling::oauth {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;
namespace ssl = ::boost::asio::ssl;

mock_telegram_server::mock_telegram_server(bool configure_flags)
    : _acceptor(_ioc, {asio::ip::make_address("127.0.0.1"), 0}),
      _ssl_ctx(ssl::context::tlsv12_server) {
  _port = _acceptor.local_endpoint().port();
  _ssl_ctx.use_certificate_chain_file(
      runfile("howling-trader/net/local.wolfe.dev.crt"));
  _ssl_ctx.use_private_key_file(
      runfile("howling-trader/net/local.wolfe.dev.key"), ssl::context::pem);

  if (configure_flags) {
    absl::SetFlag(&FLAGS_telegram_bot_token, std::string{BOT_TOKEN});
    absl::SetFlag(&FLAGS_telegram_chat_id, std::string{CHAT_ID});
    absl::SetFlag(&FLAGS_telegram_host, "127.0.0.1");
    absl::SetFlag(&FLAGS_telegram_port, _port);
  }
}

mock_telegram_server::~mock_telegram_server() {
  _ioc.stop();
  _acceptor.close();
  if (_server_thread.joinable()) _server_thread.join();
}

void mock_telegram_server::start() {
  _server_thread = std::jthread([this](std::stop_token st) {
    try {
      asio::ip::tcp::socket socket(_ioc);
      _acceptor.accept(socket);

      ssl::stream<beast::tcp_stream> stream(std::move(socket), _ssl_ctx);
      stream.handshake(ssl::stream_base::server);

      beast::flat_buffer buffer;
      http::request<http::string_body> req;
      http::read(stream, buffer, req);

      _last_request_body = req.body();
      _last_request_target = std::string{req.target()};

      http::response<http::string_body> res{http::status::ok, req.version()};
      res.set(http::field::content_type, "application/json");
      res.body() = "{\"ok\":true}";

      res.prepare_payload();
      http::write(stream, res);

      beast::error_code ec;
      stream.shutdown(ec);
    } catch (const std::exception& e) {
      // Mock server errors are expected in some test shutdown scenarios.
    }
  });
}

} // namespace howling::oauth
