#include "api/schwab/connect.h"

#include <stdexcept>
#include <string>
#include <utility>

#include "absl/flags/flag.h"

ABSL_FLAG(
    std::string,
    schwab_api_host,
    "api.schwabapi.com",
    "Hostname for the Schwab API.");

namespace howling::schwab {

std::string get_schwab_host() {
  [[maybe_unused]] static const bool _checked = ([]() {
    if (absl::GetFlag(FLAGS_schwab_api_host).empty()) {
      throw std::runtime_error("--schwab_api_host flag is required.");
    }
    return true;
  })();
  return absl::GetFlag(FLAGS_schwab_api_host);
}

net::url make_net_url(std::string target) {
  return {
      .service = "443", .host = get_schwab_host(), .target = std::move(target)};
}

} // namespace howling::schwab
