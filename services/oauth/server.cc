#include <memory>
#include <string>

#include "absl/log/log.h"
#include "environment/init.h"
#include "grpcpp/grpcpp.h"
#include "grpcpp/security/server_credentials.h"
#include "services/oauth/auth_service.h"

namespace howling {
namespace {

void run_server() {
  std::string server_address("0.0.0.0:50051");
  auth_service service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  LOG(INFO) << "gRPC Server listening on " << server_address;
  server->Wait();
}

} // namespace
} // namespace howling

int main(int argc, char** argv) {
  howling::init(argc, argv);
  howling::run_server();
  return 0;
}