#pragma once

#include <memory>

#include "boost/asio/io_context.hpp"

namespace howling {

class oauth_http_service {
public:
  oauth_http_service(boost::asio::io_context& ioc, unsigned short port);
  ~oauth_http_service();

  void start();

private:
  struct implementation;
  std::unique_ptr<implementation> _implementation;
};

} // namespace howling
