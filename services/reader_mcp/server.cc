#include <algorithm>
#include <csignal>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "boost/asio/io_context.hpp"
#include "boost/asio/signal_set.hpp"
#include "boost/system/error_code.hpp"
#include "environment/init.h"
#include "services/db/environment.h"
#include "services/db/reader/reader_db.h"
#include "services/db/reader/register.h"
#include "services/mcp/http_server.h"
#include "services/reader_mcp/service.h"
#include "services/registry/registry.h"
#include "signaling/signal_handler.h"

ABSL_FLAG(
    int, port, 8080, "TCP port on which the reader MCP HTTP server listens.");
ABSL_FLAG(
    std::string,
    bind_address,
    "127.0.0.1",
    "IP address to bind the reader MCP HTTP server to.");

namespace howling::reader_mcp {
namespace {

void run_server() {
  const int port = absl::GetFlag(FLAGS_port);
  if (port < 0 || port > 65535) {
    throw std::invalid_argument(
        "Flag --port must be between 0 and 65535, got " + std::to_string(port) +
        ".");
  }

  LOG(INFO) << "Reader MCP server starting...";
  register_reader_db();
  reader_db_pool& db_pool = registry::get_service<reader_db_pool>();
  reader_mcp_service service(db_pool);

  // Size worker threads 1:1 with the maximum database connection pool size so
  // concurrent request handling matches available database connections.
  const std::size_t worker_threads =
      std::max<std::size_t>(1, absl::GetFlag(FLAGS_db_pool_max_size));

  boost::asio::io_context io_context;
  mcp::mcp_http_server server(
      io_context,
      service.dispatcher(),
      mcp::http_server_options{
          .bind_address = absl::GetFlag(FLAGS_bind_address),
          .port = static_cast<unsigned short>(port),
          .worker_threads = worker_threads});

  boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait(
      [&io_context](const boost::system::error_code& error, int signal_number) {
        if (!error) {
          LOG(INFO) << "Received signal " << signal_number
                    << ", shutting down reader MCP server.";
          io_context.stop();
        }
      });

  if (signal_handler::instance().has_received_signal()) {
    LOG(INFO) << "Received signal "
              << signal_handler::instance().received_signal()
              << " during startup, skipping serve loop.";
  } else {
    server.start();
    LOG(INFO) << "Reader MCP HTTP server listening on "
              << absl::GetFlag(FLAGS_bind_address) << ":" << server.bound_port()
              << " with " << worker_threads << " worker thread(s).";
    io_context.run();
  }
  server.stop();
  LOG(INFO) << "Reader MCP server stopped cleanly.";
}

} // namespace
} // namespace howling::reader_mcp

int main(int argc, char** argv) {
  try {
    howling::init(argc, argv);
    howling::reader_mcp::run_server();
  } catch (const std::exception& error) {
    LOG(FATAL) << "Unhandled exception: " << error.what();
  } catch (...) { LOG(FATAL) << "Unhandled exception of unknown type."; }
  return 0;
}
