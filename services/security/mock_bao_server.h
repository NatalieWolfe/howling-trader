#pragma once

#include <memory>
#include <string>
#include <thread>

#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"

namespace howling::security {

class mock_bao_server {
public:
  static constexpr std::string_view CLIENT_TOKEN = "test_client_token";

  explicit mock_bao_server(bool configure_flags = true);
  ~mock_bao_server();

  void start();

  unsigned short port() const { return _port; }
  const std::string& last_request_body() const { return _last_request_body; }

private:
  boost::asio::io_context _ioc;
  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _ssl_ctx;
  unsigned short _port;
  std::string _last_request_body;
  std::jthread _server_thread;
};

} // namespace howling::security
