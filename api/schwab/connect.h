#pragma once

#include <functional>
#include <string>

#include "net/url.h"

namespace howling::schwab {

/**
 * @brief Connection metadata and credentials for a Schwab streaming session.
 */
struct streamer_session_info {
  net::url socket_url;
  std::string customer_id;
  std::string correlation_id;
  std::string channel;
  std::string function_id;
  std::string bearer_token;
};

using streamer_session_provider = std::function<streamer_session_info()>;

/**
 * @brief Sets or clears a custom streamer session provider override.
 */
void set_streamer_session_provider(streamer_session_provider provider);

/**
 * @brief Fetches streamer session info using the configured provider override,
 * or `default_provider` when no override is installed.
 */
streamer_session_info fetch_streamer_session_info(
    streamer_session_provider default_provider = nullptr);

/**
 * @brief Scoped RAII override for the streamer session provider used in tests.
 */
class scoped_streamer_session_provider {
public:
  explicit scoped_streamer_session_provider(streamer_session_provider provider);
  scoped_streamer_session_provider(const scoped_streamer_session_provider&) =
      delete;
  scoped_streamer_session_provider&
  operator=(const scoped_streamer_session_provider&) = delete;
  ~scoped_streamer_session_provider();

private:
  streamer_session_provider _previous;
};

std::string get_schwab_host();

net::url make_net_url(std::string target);

} // namespace howling::schwab
