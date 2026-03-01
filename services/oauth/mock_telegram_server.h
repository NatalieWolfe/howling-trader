#pragma once

#include <memory>
#include <string>
#include <thread>

#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/beast.hpp"

namespace howling::oauth {

class mock_telegram_server {
public:
  static constexpr std::string_view BOT_TOKEN = "test_bot_token";
  static constexpr std::string_view CHAT_ID = "test_chat_id";

  explicit mock_telegram_server(bool configure_flags = true);
  ~mock_telegram_server();

  void start();

  unsigned short port() const { return _port; }
  const std::string& last_request_body() const { return _last_request_body; }
  const std::string& last_request_target() const {
    return _last_request_target;
  }

private:
  boost::asio::io_context _ioc;
  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ssl::context _ssl_ctx;
  unsigned short _port;
  std::string _last_request_body;
  std::string _last_request_target;
  std::jthread _server_thread;
};

} // namespace howling::oauth
