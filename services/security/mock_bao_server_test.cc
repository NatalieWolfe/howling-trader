#include "services/security/mock_bao_server.h"

#include "boost/asio/buffer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast/core/error.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http.hpp"
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace howling::security {
namespace {

using ::testing::HasSubstr;

TEST(MockBaoServerTest, InitializesAndCompletesVerifiedTlsHandshake) {
  namespace asio = ::boost::asio;
  namespace beast = ::boost::beast;
  namespace http = ::boost::beast::http;

  mock_bao_server server(/*configure_flags=*/false, /*use_ssl=*/true);
  ASSERT_FALSE(server.ca_cert_pem().empty());
  server.start();

  asio::io_context io_context;
  asio::ssl::context ssl_context{asio::ssl::context::tls_client};
  ssl_context.add_certificate_authority(asio::buffer(server.ca_cert_pem()));
  ssl_context.set_verify_mode(asio::ssl::verify_peer);

  asio::ssl::stream<beast::tcp_stream> stream{io_context, ssl_context};
  ASSERT_EQ(
      X509_VERIFY_PARAM_set1_ip_asc(
          SSL_get0_param(stream.native_handle()), "127.0.0.1"),
      1);
  beast::get_lowest_layer(stream).connect(
      asio::ip::tcp::endpoint{
          asio::ip::make_address("127.0.0.1"), server.port()});
  ASSERT_NO_THROW(stream.handshake(asio::ssl::stream_base::client));

  http::request<http::string_body> request{
      http::verb::get, "/v1/sys/health", /*version=*/11};
  request.set(http::field::host, "127.0.0.1");
  http::write(stream, request);

  beast::flat_buffer buffer;
  http::response<http::string_body> response;
  http::read(stream, buffer, response);
  EXPECT_EQ(response.result(), http::status::ok);
  EXPECT_THAT(response.body(), HasSubstr("\"initialized\": true"));

  beast::error_code error_code;
  stream.shutdown(error_code);
}

} // namespace
} // namespace howling::security
