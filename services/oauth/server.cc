#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "api/schwab/configuration.h"
#include "boost/asio/io_context.hpp"
#include "environment/init.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/server_credentials.h"
#include "services/db/make_database.h"
#include "services/oauth/auth_service.h"
#include "services/oauth/oauth_exchanger_impl.h"
#include "services/oauth/oauth_http_service.h"
#include "services/registry/registry.h"
#include "services/security.h"
#include "services/security/register.h"

ABSL_DECLARE_FLAG(std::string, telegram_bot_token);
ABSL_DECLARE_FLAG(std::string, telegram_chat_id);

namespace howling {
namespace {

using namespace std::chrono_literals;

// TODO: Take the port from a flag.
constexpr std::string_view GRPC_ADDRESS = "0.0.0.0:50051";
constexpr unsigned short HTTP_PORT = 8080;

void run_server() {
  LOG(INFO) << "Oauth server starting...";

  security::register_security_client();
  schwab::fetch_schwab_secrets();

  Json::Value telegram_secret =
      registry::get_service<security_client>().get_secret(
          "howling/prod/telegram");
  absl::SetFlag(&FLAGS_telegram_bot_token, telegram_secret["token"].asString());
  absl::SetFlag(&FLAGS_telegram_chat_id, telegram_secret["chat_id"].asString());

  // TODO: Add a database connection pool. The pool should check that
  // connections are still valid before passing them to callers. The connection
  // should auto-release back to the pool upon destruction.
  std::unique_ptr<database> db = make_database();
  db->check_schema_version().get();
  LOG(INFO) << "Database connection established.";

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
  // TODO: Use secure server credentials. Use the howling::security::bao_client
  // to retrieve and manage TLS certificates sourced from OpenBao.
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
  try {
    howling::init(argc, argv);
    howling::run_server();
  } catch (const std::exception& e) {
    LOG(FATAL) << "Unhandled exception: " << e.what();
  }
  return 0;
}
