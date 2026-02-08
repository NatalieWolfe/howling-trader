load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "auth_service_lib",
    srcs = ["auth_service.cc"],
    hdrs = ["auth_service.h"],
    deps = [
        "//services/oauth/proto:auth_service_cc_grpc",
        "@abseil-cpp//absl/log",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    deps = [
        ":auth_service_lib",
        "//environment:init",
        "@abseil-cpp//absl/log",
        "@grpc//:grpc++",
    ],
)

cc_test(
    name = "auth_service_test",
    srcs = ["auth_service_test.cc"],
    deps = [
        ":auth_service_lib",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@googletest//:gtest_main",
        "@grpc//:grpc++",
    ],
)