#pragma once

#include "google/protobuf/empty.pb.h"
#include "grpcpp/grpcpp.h"
#include "services/oauth/proto/auth_service.grpc.pb.h"
#include "services/oauth/proto/auth_service.pb.h"
#include "gmock/gmock.h"

namespace howling {

class MockAuthServiceStub : public AuthService::StubInterface {
public:
  MOCK_METHOD(
      grpc::Status,
      RequestLogin,
      (grpc::ClientContext * context,
       const LoginRequest& request,
       google::protobuf::Empty* response),
      (override));

  // Async methods must also be overridden if called, but we only use the
  // synchronous one. StubInterface is abstract, so we must override all pure
  // virtual methods.

  grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*
  AsyncRequestLoginRaw(
      grpc::ClientContext* context,
      const LoginRequest& request,
      grpc::CompletionQueue* cq) override {
    return nullptr;
  }

  grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*
  PrepareAsyncRequestLoginRaw(
      grpc::ClientContext* context,
      const LoginRequest& request,
      grpc::CompletionQueue* cq) override {
    return nullptr;
  }
};

} // namespace howling
