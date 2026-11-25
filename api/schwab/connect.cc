#include "api/schwab/connect.h"

#include <functional>
#include <mutex>
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
namespace {

std::mutex& session_provider_mutex() {
  static std::mutex mutex;
  return mutex;
}

streamer_session_provider& custom_session_provider() {
  static streamer_session_provider provider;
  return provider;
}

streamer_session_provider
swap_session_provider(streamer_session_provider provider) {
  std::lock_guard lock{session_provider_mutex()};
  return std::exchange(custom_session_provider(), std::move(provider));
}

} // namespace

void set_streamer_session_provider(streamer_session_provider provider) {
  swap_session_provider(std::move(provider));
}

streamer_session_info
fetch_streamer_session_info(streamer_session_provider default_provider) {
  streamer_session_provider provider;
  {
    std::lock_guard lock{session_provider_mutex()};
    provider = custom_session_provider();
  }
  if (provider) return provider();
  if (default_provider) return default_provider();
  throw std::runtime_error("No Schwab streamer session provider configured.");
}

scoped_streamer_session_provider::scoped_streamer_session_provider(
    streamer_session_provider provider)
    : _previous{swap_session_provider(std::move(provider))} {}

scoped_streamer_session_provider::~scoped_streamer_session_provider() {
  swap_session_provider(std::move(_previous));
}

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
