#pragma once

#include <atomic>
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

  explicit mock_bao_server(bool configure_flags = true, bool use_ssl = true);
  ~mock_bao_server();

  void start();
  void set_secret_not_found_count(int count) {
    _secret_not_found_count = count;
  }

  unsigned short port() const { return _port; }
  const std::string& last_request_body() const { return _last_request_body; }

private:
  boost::asio::io_context _ioc;
  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _ssl_ctx;
  bool _use_ssl;
  unsigned short _port;
  int _secret_not_found_count = 0;
  std::atomic<bool> _stopped = false;
  std::string _last_request_body;
  std::thread _server_thread;
};

} // namespace howling::security
