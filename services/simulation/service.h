#pragma once

#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"
#include "services/database.h"
#include "services/proto/simulation_service.grpc.pb.h"
#include "services/proto/simulation_service.pb.h"

namespace howling {

class simulation_service final : public SimulationService::Service {
public:
  explicit simulation_service(database_pool& db_pool) : _db_pool{db_pool} {}

  grpc::Status Simulate(
      grpc::ServerContext* context,
      const SimulateRequest* request,
      SimulateResponse* response) override;

private:
  database_pool& _db_pool;
};

} // namespace howling
