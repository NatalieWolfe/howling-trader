#include <exception>
#include <memory>
#include <string>
#include <string_view>

#include "absl/log/log.h"
#include "environment/init.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/server_credentials.h"
#include "services/database.h"
#include "services/db/register.h"
#include "services/registry/registry.h"
#include "services/security.h"
#include "services/security/certificate_bundle.h"
#include "services/security/register.h"
#include "services/simulation/service.h"

namespace howling {
namespace {

// TODO: Take the port from a flag.
constexpr std::string_view GRPC_ADDRESS = "0.0.0.0:50051";

auto make_server_credentials() {
  // TODO: Manage the certificate role and common name more flexibly using flags
  // or other configuration values.
  certificate_bundle grpc_cert =
      registry::get_service<security_client>().create_certificate(
          "howling-node-role",
          "howling-simulation.howling-app.svc.cluster.local");
  grpc::SslServerCredentialsOptions ssl_options;
  ssl_options.pem_root_certs = grpc_cert.ca_chain;
  ssl_options.pem_key_cert_pairs.push_back(
      {.private_key = grpc_cert.private_key,
       .cert_chain = grpc_cert.certificate});
  return grpc::SslServerCredentials(ssl_options);
}

void run_server() {
  LOG(INFO) << "Simulation server starting...";

  security::register_security_client();
  register_database_client();
  database_pool& db_pool = registry::get_service<database_pool>();

  simulation_service service{db_pool};
  grpc::ServerBuilder builder;
  builder.AddListeningPort(
      std::string{GRPC_ADDRESS}, make_server_credentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
  LOG(INFO) << "gRPC Server listening on " << GRPC_ADDRESS;
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
  } catch (...) { LOG(FATAL) << "Unhandled exception of unknown type"; }
  return 0;
}
