#pragma once

#include "google/protobuf/empty.pb.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "services/database.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"

namespace howling {

class auth_service final : public AuthService::Service {
public:
  explicit auth_service(database& db) : _db{db} {}

  grpc::Status RequestLogin(
      grpc::ServerContext* context,
      const LoginRequest* request,
      google::protobuf::Empty* response) override;

private:
  database& _db;
};

} // namespace howling
