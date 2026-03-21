#pragma once

#include <memory>

#include "grpcpp/grpcpp.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"

namespace howling {

std::unique_ptr<AuthService::Stub>
make_auth_service_stub(std::shared_ptr<grpc::Channel> channel = nullptr);

}
