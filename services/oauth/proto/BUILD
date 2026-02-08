load("@rules_proto//proto:defs.bzl", "proto_library")
load("@grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "auth_service_proto",
    srcs = ["auth_service.proto"],
    deps = ["@protobuf//:empty_proto"],
)

cc_proto_library(
    name = "auth_service_cc_proto",
    deps = [":auth_service_proto"],
)

cc_grpc_library(
    name = "auth_service_cc_grpc",
    srcs = [":auth_service_proto"],
    grpc_only = True,
    deps = [":auth_service_cc_proto"],
)
