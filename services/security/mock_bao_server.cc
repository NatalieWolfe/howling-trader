#include "services/security/mock_bao_server.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"
#include "environment/runfiles.h"

ABSL_DECLARE_FLAG(std::string, bao_address);

namespace howling::security {
namespace {

namespace asio = ::boost::asio;
namespace beast = ::boost::beast;
namespace http = ::boost::beast::http;
namespace ssl = ::boost::asio::ssl;

template <typename Stream>
void handle_request(Stream& stream, std::string* last_request_body) {
  beast::flat_buffer buffer;
  http::request<http::string_body> req;
  beast::error_code ec;
  http::read(stream, buffer, req, ec);
  if (ec) return;

  *last_request_body = req.body();

  http::response<http::string_body> res{http::status::ok, req.version()};
  res.set(http::field::content_type, "application/json");

  if (req.target() == "/v1/sys/health") {
    res.body() = "{\"initialized\": true, \"sealed\": false}";
  } else if (req.target().starts_with("/v1/secret/data/")) {
    res.body() = R"json({"data":{"data":{"key":"value"}}})json";
  } else if (req.target().starts_with("/v1/transit/encrypt/")) {
    res.body() =
        R"json({"data":{"ciphertext":"vault:v1:test_ciphertext"}})json";
  } else if (req.target().starts_with("/v1/transit/decrypt/")) {
    // "test_plaintext" in base64
    res.body() = R"json({"data":{"plaintext":"dGVzdF9wbGFpbnRleHQ="}})json";
  } else {
    res.body() = absl::StrCat(
        "{\"auth\": {\"client_token\": \"",
        mock_bao_server::CLIENT_TOKEN,
        "\"}}");
  }

  res.prepare_payload();
  http::write(stream, res, ec);
  if (ec) return;

  if constexpr (std::is_same_v<Stream, ssl::stream<beast::tcp_stream>>) {
    stream.shutdown(ec);
  } else {
    stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  }
}

} // namespace

mock_bao_server::mock_bao_server(bool configure_flags, bool use_ssl)
    : _acceptor(_ioc, {asio::ip::make_address("127.0.0.1"), 0}),
      _ssl_ctx(ssl::context::tlsv12_server), _use_ssl(use_ssl) {
  _port = _acceptor.local_endpoint().port();
  if (_use_ssl) {
    _ssl_ctx.use_certificate_chain_file(
        runfile("howling-trader/net/local.wolfe.dev.crt"));
    _ssl_ctx.use_private_key_file(
        runfile("howling-trader/net/local.wolfe.dev.key"), ssl::context::pem);
  }

  if (configure_flags) {
    absl::SetFlag(
        &FLAGS_bao_address,
        absl::StrCat(_use_ssl ? "https" : "http", "://127.0.0.1:", _port));
  }
}

mock_bao_server::~mock_bao_server() {
  beast::error_code ec;
  _acceptor.cancel(ec);
  _acceptor.close(ec);
  if (_server_thread.joinable()) _server_thread.join();
}

void mock_bao_server::start() {
  _server_thread = std::thread([this]() {
    try {
      beast::error_code ec;
      asio::ip::tcp::socket socket(_ioc);
      _acceptor.accept(socket, ec);
      if (ec) return;

      if (_use_ssl) {
        ssl::stream<beast::tcp_stream> stream(std::move(socket), _ssl_ctx);
        stream.handshake(ssl::stream_base::server, ec);
        if (ec) return;
        handle_request(stream, &_last_request_body);
      } else {
        beast::tcp_stream stream(std::move(socket));
        handle_request(stream, &_last_request_body);
      }
    } catch (...) {}
  });
}

} // namespace howling::security
