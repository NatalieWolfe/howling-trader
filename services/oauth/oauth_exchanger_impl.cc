#include "services/oauth/oauth_exchanger_impl.h"

#include <memory>

#include "api/schwab/connect.h"
#include "api/schwab/oauth.h"
#include "net/connect.h"

namespace howling {

schwab::oauth_tokens oauth_exchanger_impl::exchange(std::string_view code) {
  std::unique_ptr<net::connection> conn =
      net::make_connection(schwab::make_net_url("/"));
  return schwab::exchange_code_for_tokens(*conn, code);
}

} // namespace howling