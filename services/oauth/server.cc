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
#include "services/database.h"
#include "services/db/register.h"
#include "services/oauth/auth_service.h"
#include "services/oauth/oauth_exchanger_impl.h"
#include "services/oauth/oauth_http_service.h"
#include "services/registry/registry.h"
#include "services/security.h"
#include "services/security/certificate_bundle.h"
#include "services/security/register.h"

ABSL_DECLARE_FLAG(std::string, telegram_bot_token);
ABSL_DECLARE_FLAG(std::string, telegram_chat_id);

namespace howling {
namespace {

using namespace std::chrono_literals;

// TODO: Take the port from a flag.
constexpr std::string_view GRPC_ADDRESS = "0.0.0.0:50051";
constexpr unsigned short HTTP_PORT = 8080;

auto make_server_credentials() {
  // TODO: Manage the certificate role and common name more flexibly using flags
  // or other configuration values.
  certificate_bundle grpc_cert =
      registry::get_service<security_client>().create_certificate(
          "howling-node-role", "howling-oauth.howling-app.svc.cluster.local");
  grpc::SslServerCredentialsOptions ssl_options;
  ssl_options.pem_root_certs = grpc_cert.ca_chain;
  ssl_options.pem_key_cert_pairs.push_back(
      {.private_key = grpc_cert.private_key,
       .cert_chain = grpc_cert.certificate});
  return grpc::SslServerCredentials(ssl_options);
}

void run_server() {
  LOG(INFO) << "Oauth server starting...";

  security::register_security_client();
  register_database_client();
  schwab::fetch_schwab_secrets();

  Json::Value telegram_secret =
      registry::get_service<security_client>().get_secret(
          "howling/prod/telegram");
  absl::SetFlag(&FLAGS_telegram_bot_token, telegram_secret["token"].asString());
  absl::SetFlag(&FLAGS_telegram_chat_id, telegram_secret["chat_id"].asString());

  database& db = registry::get_service<database>();
  boost::asio::io_context ioc{/*concurrency_hint=*/1};
  oauth_http_service http_service(
      ioc, HTTP_PORT, db, std::make_unique<oauth_exchanger_impl>());
  http_service.start();

  std::jthread http_thread([&ioc]() {
    LOG(INFO) << "HTTP Server starting...";
    ioc.run();
  });

  auth_service service(db);
  grpc::ServerBuilder builder;
  builder.AddListeningPort(
      std::string{GRPC_ADDRESS}, make_server_credentials());
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
