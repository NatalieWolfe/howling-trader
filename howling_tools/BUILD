load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//infrastructure:containers.bzl", "package_binary")

cc_binary(
    name = "evaluate",
    srcs = ["evaluate.cc"],
    data = ["//data:history"],
    deps = [
        "//cli:printing",
        "//containers:vector",
        "//data:aggregate",
        "//data:analyzer",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:init",
        "//environment:runfiles",
        "//services:database",
        "//services:security",
        "//services/db:register",
        "//services/registry",
        "//services/security:register",
        "//time:conversion",
        "//trading:metrics",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_binary(
    name = "execute",
    srcs = ["execute.cc"],
    deps = [
        ":session_shutdown",
        "//api:schwab",
        "//cli:printing",
        "//containers:vector",
        "//data:account_cc_proto",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:configuration",
        "//environment:init",
        "//files",
        "//services:database",
        "//services:market_watch",
        "//services:security",
        "//services/db:register",
        "//services/registry",
        "//services/security:register",
        "//signaling:cancellation",
        "//strings:format",
        "//time:conversion",
        "//trading:executor",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
    ],
)

package_binary(
    name = "execute_image",
    image_name = "howling-execute",
    binary = ":execute",
    visibility = ["//infrastructure:visibility"],
)

cc_binary(
    name = "fetch",
    srcs = ["fetch.cc"],
    deps = [
        "//api:alpaca",
        "//api:schwab",
        "//containers:vector",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:init",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@protobuf",
    ],
)

cc_library(
    name = "session_shutdown",
    srcs = ["session_shutdown.cc"],
    hdrs = ["session_shutdown.h"],
    deps = [
        "//functional:retry",
        "//signaling:cancellation",
    ],
)

cc_test(
    name = "session_shutdown_test",
    srcs = ["session_shutdown_test.cc"],
    deps = [
        ":session_shutdown",
        "//signaling:cancellation",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
    name = "visualize",
    srcs = ["visualize.cc"],
    data = ["//data:history"],
    deps = [
        "//cli:printing",
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:init",
        "//environment:runfiles",
        "//time:conversion",
        "//trading:metrics",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)
