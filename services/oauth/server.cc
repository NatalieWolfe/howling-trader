#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "absl/log/log.h"
#include "boost/asio/io_context.hpp"
#include "environment/init.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/server_credentials.h"
#include "services/db/make_database.h"
#include "services/oauth/auth_service.h"
#include "services/oauth/oauth_http_service.h"
#include "services/oauth/oauth_exchanger_impl.h"

namespace howling {
namespace {

// TODO: Take the port from a flag.
constexpr std::string_view GRPC_ADDRESS = "0.0.0.0:50051";
constexpr unsigned short HTTP_PORT = 8080;

void run_server() {
  // TODO: Add a database connection pool. The pool should check that
  // connections are still valid before passing them to callers. The connection
  // should auto-release back to the pool upon destruction.
  std::unique_ptr<database> db = make_database();

  boost::asio::io_context ioc{/*concurrency_hint=*/1};
  oauth_http_service http_service(
      ioc, HTTP_PORT, *db, std::make_unique<oauth_exchanger_impl>());
  http_service.start();

  std::jthread http_thread([&ioc]() {
    LOG(INFO) << "HTTP Server starting...";
    ioc.run();
  });

  auth_service service(*db);
  grpc::ServerBuilder builder;
  // TODO: Use secure server credentials.
  builder.AddListeningPort(
      std::string{GRPC_ADDRESS}, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  LOG(INFO) << "gRPC Server listening on " << GRPC_ADDRESS;
  LOG(INFO) << "HTTP Server listening on port " << HTTP_PORT;

  server->Wait();
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);
  howling::run_server();
  return 0;
}
