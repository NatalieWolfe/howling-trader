#pragma once

#include <memory>

#include "api/schwab/oauth.h"
#include "boost/asio/io_context.hpp"
#include "net/connect.h"
#include "services/database.h"

#include "services/oauth/oauth_exchanger.h"

namespace howling {

class oauth_http_service {
public:
  oauth_http_service(
      boost::asio::io_context& ioc,
      unsigned short port,
      database& db,
      std::unique_ptr<oauth_exchanger> exchanger);
  ~oauth_http_service();

  void start();

private:
  struct implementation;
  std::unique_ptr<implementation> _implementation;
};

} // namespace howling
