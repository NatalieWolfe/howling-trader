load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//infrastructure:containers.bzl", "package_binary")

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    deps = [
        ":service",
        "//environment:init",
        "//services:database",
        "//services:security",
        "//services/db:register",
        "//services/registry",
        "//services/security:certificate_bundle",
        "//services/security:register",
        "@abseil-cpp//absl/log",
        "@grpc//:grpc++",
    ],
)

package_binary(
    name = "server_image",
    binary = ":server",
    image_name = "howling-simulation-service",
    visibility = ["//infrastructure:visibility"],
)

cc_library(
    name = "service",
    srcs = ["service.cc"],
    hdrs = ["service.h"],
    deps = [
        "//data:aggregate",
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services:database",
        "//services/proto:simulation_service_cc_grpc",
        "//services/proto:simulation_service_cc_proto",
        "//time:conversion",
        "//trading:executor",
        "//trading:metrics",
        "//trading:trading_state",
        "@grpc//:grpc++",
    ],
)
