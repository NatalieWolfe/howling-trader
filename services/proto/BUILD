load("@grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")

proto_library(
    name = "simulation_service_proto",
    srcs = ["simulation_service.proto"],
    deps = [
        "//data:analyzer_proto",
        "//data:stock_proto",
        "//data:trade_proto",
        "@protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "simulation_service_cc_proto",
    visibility = ["//services/simulation:__subpackages__"],
    deps = [":simulation_service_proto"],
)

cc_grpc_library(
    name = "simulation_service_cc_grpc",
    srcs = [":simulation_service_proto"],
    grpc_only = True,
    visibility = ["//services/simulation:__subpackages__"],
    deps = [":simulation_service_cc_proto"],
)
