load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "tool_handler",
    hdrs = ["tool_handler.h"],
    deps = ["@jsoncpp"],
)

cc_library(
    name = "resource_handler",
    hdrs = ["resource_handler.h"],
)

cc_library(
    name = "dispatcher",
    srcs = ["dispatcher.cc"],
    hdrs = ["dispatcher.h"],
    deps = [
        ":resource_handler",
        ":tool_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_library(
    name = "http_server",
    srcs = ["http_server.cc"],
    hdrs = ["http_server.h"],
    deps = [
        ":dispatcher",
        "//strings:json",
        "@abseil-cpp//absl/log",
        "@boost.asio",
        "@boost.beast",
        "@jsoncpp",
    ],
)

cc_test(
    name = "dispatcher_test",
    srcs = ["dispatcher_test.cc"],
    deps = [
        ":dispatcher",
        ":resource_handler",
        ":tool_handler",
        "//strings:json",
        "@googletest//:gtest_main",
        "@jsoncpp",
    ],
)

cc_test(
    name = "http_server_test",
    srcs = ["http_server_test.cc"],
    deps = [
        ":dispatcher",
        ":http_server",
        ":resource_handler",
        ":tool_handler",
        "//strings:json",
        "@boost.asio",
        "@boost.beast",
        "@googletest//:gtest_main",
        "@jsoncpp",
    ],
)
