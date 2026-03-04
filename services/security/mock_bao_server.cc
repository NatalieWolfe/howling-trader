#include "services/security/mock_bao_server.h"

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

ABSL_DECLARE_FLAG(std::string, bao_address);
ABSL_DECLARE_FLAG(std::string, bao_auth_path);

namespace howling::security {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;
namespace ssl = ::boost::asio::ssl;

mock_bao_server::mock_bao_server(bool configure_flags)
    : _acceptor(_ioc, {asio::ip::make_address("127.0.0.1"), 0}),
      _ssl_ctx(ssl::context::tlsv12_server) {
  _port = _acceptor.local_endpoint().port();
  _ssl_ctx.use_certificate_chain_file(
      runfile("howling-trader/net/local.wolfe.dev.crt"));
  _ssl_ctx.use_private_key_file(
      runfile("howling-trader/net/local.wolfe.dev.key"), ssl::context::pem);

  if (configure_flags) {
    absl::SetFlag(
        &FLAGS_bao_address, "https://127.0.0.1:" + std::to_string(_port));
    absl::SetFlag(&FLAGS_bao_auth_path, "auth/kubernetes/login");
  }
}

mock_bao_server::~mock_bao_server() {
  _ioc.stop();
  _acceptor.close();
  if (_server_thread.joinable()) _server_thread.join();
}

void mock_bao_server::start() {
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

      http::response<http::string_body> res{http::status::ok, req.version()};
      res.set(http::field::content_type, "application/json");
      res.body() = "{\"auth\": {\"client_token\": \"" +
          std::string{CLIENT_TOKEN} + "\"}}";

      res.prepare_payload();
      http::write(stream, res);

      beast::error_code ec;
      stream.shutdown(ec);
    } catch (const std::exception& e) {
      // Mock server errors are expected in some test shutdown scenarios.
    }
  });
}

} // namespace howling::security
