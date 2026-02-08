#include "api/schwab/oauth.h"

#include "api/schwab/connect.h"
#include "boost/url/url.hpp"

namespace howling::schwab {

std::string make_schwab_authorize_url(
    std::string_view client_id, std::string_view redirect_url) {
  boost::urls::url url;
  url.set_scheme("https");
  url.set_host(get_schwab_host());
  url.set_path("/v1/oauth/authorize");
  url.params().append({"client_id", client_id});
  url.params().append({"redirect_uri", redirect_url});
  url.params().append({"response_type", "code"});
  return url.c_str();
}

} // namespace howling::schwab
