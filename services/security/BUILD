load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "bao_client",
    srcs = ["bao_client.cc"],
    hdrs = ["bao_client.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":certificate_bundle",
        "//files",
        "//net:connect",
        "//net:request",
        "//net:url",
        "//services:security",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@boost.asio",
        "@boost.beast",
        "@boost.url",
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
        ":certificate_bundle",
        ":mock_bao_server",
        "//environment:runfiles",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_bao_server",
    testonly = True,
    srcs = ["mock_bao_server.cc"],
    hdrs = ["mock_bao_server.h"],
    data = [
        "//net:local.wolfe.dev.crt",
        "//net:local.wolfe.dev.key",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//environment:runfiles",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
        "@boost.asio",
        "@boost.beast",
    ],
)

cc_library(
    name = "certificate_bundle",
    hdrs = ["certificate_bundle.h"],
    visibility = ["//services:__subpackages__"],
)

cc_library(
    name = "register",
    srcs = ["register.cc"],
    hdrs = ["register.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":bao_client",
        "//services/registry:register_service",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/time",
    ],
)
