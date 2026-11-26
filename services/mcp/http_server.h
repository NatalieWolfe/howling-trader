#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "boost/asio/io_context.hpp"
#include "services/mcp/dispatcher.h"

namespace howling::mcp {

inline constexpr std::size_t DEFAULT_MAX_BODY_BYTES = 1024 * 1024;
inline constexpr std::size_t DEFAULT_WORKER_THREADS = 4;
inline constexpr std::chrono::seconds DEFAULT_REQUEST_TIMEOUT =
    std::chrono::seconds(60);

/**
 * @brief Configuration options for `mcp_http_server`.
 */
struct http_server_options {
  // Numeric IP address literal to bind (e.g., "127.0.0.1" or "::1").
  // Hostnames such as "localhost" are not accepted.
  std::string bind_address = "127.0.0.1";
  unsigned short port = 0;
  std::size_t worker_threads = DEFAULT_WORKER_THREADS;
  std::size_t max_body_bytes = DEFAULT_MAX_BODY_BYTES;
  std::chrono::seconds request_timeout = DEFAULT_REQUEST_TIMEOUT;
};

/**
 * @brief Returns true if an HTTP Origin header value is a loopback origin.
 *
 * Accepts `http` and `https` schemes with host `localhost`, `127.0.0.1`, or
 * `[::1]`, with an optional valid port. Rejects paths, credentials, and
 * non-loopback hosts.
 */
[[nodiscard]] bool is_allowed_loopback_origin(std::string_view origin);

/**
 * @brief Stateless MCP Streamable HTTP server built on Boost.Beast and Asio.
 *
 * Asynchronous network I/O runs on the caller-provided `io_context` with
 * per-connection strands. `POST /mcp` JSON-RPC dispatch runs on an internal
 * `boost::asio::thread_pool`, and responses are posted back onto the
 * connection's strand so `GET /status` and HTTP I/O are never blocked by slow
 * handlers.
 *
 * Thread safety: `start()`, `stop()`, and `bound_port()` are thread-safe.
 * `start()` is idempotent and may be called before or while `io_context` is
 * running. `stop()` is idempotent, closes the listening socket and active HTTP
 * sessions, prevents new dispatches from starting, and blocks every concurrent
 * caller until all in-flight worker-pool dispatches finish. The provided
 * `mcp_dispatcher` must outlive the server (or stay alive until `stop()`
 * returns).
 */
class mcp_http_server {
public:
  /**
   * @brief Binds the listening socket and initializes the worker pool.
   *
   * @param io_context
   *  Asio I/O context driving asynchronous accepts and socket I/O.
   * @param dispatcher
   *  MCP JSON-RPC dispatcher invoked for `POST /mcp` requests.
   * @param options
   *  Bind address (numeric IP literal), port (0 for an ephemeral port), worker
   *  thread count, body limit, and per-request timeout.
   */
  mcp_http_server(
      boost::asio::io_context& io_context,
      const mcp_dispatcher& dispatcher,
      http_server_options options = {});

  ~mcp_http_server();

  mcp_http_server(const mcp_http_server&) = delete;
  mcp_http_server& operator=(const mcp_http_server&) = delete;

  /**
   * @brief Begins accepting incoming HTTP connections asynchronously.
   *
   * Safe to call multiple times; subsequent calls are ignored.
   */
  void start();

  /**
   * @brief Stops accepting connections, closes active sessions, and joins the
   * worker thread pool.
   *
   * Safe to call concurrently and multiple times. Every caller blocks until
   * all in-flight dispatches on the worker pool have completed.
   */
  void stop();

  /**
   * @brief Returns the TCP port bound by the listening socket.
   */
  [[nodiscard]] unsigned short bound_port() const;

private:
  struct implementation;
  std::shared_ptr<implementation> _implementation;
};

} // namespace howling::mcp
