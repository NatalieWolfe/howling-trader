load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "executor",
    srcs = ["executor.cc"],
    hdrs = ["executor.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":metrics",
        ":trading_state",
        "//api:schwab",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//environment:configuration",
        "//net:connect",
    ],
)

cc_library(
    name = "metrics",
    srcs = ["metrics.cc"],
    hdrs = ["metrics.h"],
    visibility = ["//visibility:public"],
    deps = ["//containers:vector"],
)

cc_library(
    name = "pricing",
    srcs = ["pricing.cc"],
    hdrs = ["pricing.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":trading_state",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
    ],
)

cc_library(
    name = "trading_state",
    srcs = ["trading_state.cc"],
    hdrs = ["trading_state.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//containers:vector",
        "//data:aggregate",
        "//data:stock_cc_proto",
        "//time:thread_safe",
    ],
)

cc_test(
    name = "trading_state_test",
    srcs = ["trading_state_test.cc"],
    deps = [
        ":trading_state",
        "//time:thread_safe",
        "@googletest//:gtest_main",
    ],
)
