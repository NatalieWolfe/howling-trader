load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "bao_client",
    srcs = ["bao_client.cc"],
    hdrs = ["bao_client.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//net:connect",
        "//net:url",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@boost.asio",
        "@boost.beast",
        "@jsoncpp",
    ],
)

cc_test(
    name = "bao_client_test",
    srcs = ["bao_client_test.cc"],
    data = [
        "//net:local.wolfe.dev.crt",
        "//net:local.wolfe.dev.key",
    ],
    deps = [
        ":bao_client",
        "//environment:runfiles",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@googletest//:gtest_main",
    ],
)
