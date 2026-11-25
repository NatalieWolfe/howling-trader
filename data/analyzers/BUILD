load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "atr",
    srcs = ["atr.cc"],
    hdrs = ["atr.h"],
    deps = [
        "//containers:circular_buffer",
        "//containers:vector",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_utils",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:wilder_rma",
        "//trading:trading_state",
    ],
)

cc_test(
    name = "atr_test",
    srcs = ["atr_test.cc"],
    deps = [
        ":atr",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_cc_proto",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//trading:trading_state",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "bollinger",
    srcs = ["bollinger.cc"],
    hdrs = ["bollinger.h"],
    deps = [
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//trading:trading_state",
    ],
)

cc_library(
    name = "ema_ribbon",
    srcs = ["ema_ribbon.cc"],
    hdrs = ["ema_ribbon.h"],
    deps = [
        "//containers:vector",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_utils",
        "//data:stock_cc_proto",
        "//trading:trading_state",
    ],
)

cc_test(
    name = "ema_ribbon_test",
    srcs = ["ema_ribbon_test.cc"],
    deps = [
        ":ema_ribbon",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_cc_proto",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//trading:trading_state",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "gap_momentum",
    srcs = ["gap_momentum.cc"],
    hdrs = ["gap_momentum.h"],
    deps = [
        "//containers:circular_buffer",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_utils",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//time:conversion",
        "//time:market_time",
        "//trading:trading_state",
    ],
)

cc_test(
    name = "gap_momentum_test",
    srcs = ["gap_momentum_test.cc"],
    deps = [
        ":gap_momentum",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_cc_proto",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//time:conversion",
        "//trading:trading_state",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "howling",
    srcs = ["howling.cc"],
    hdrs = ["howling.h"],
    deps = [
        ":bollinger",
        ":macd",
        ":market_hours",
        ":profit",
        "//containers:circular_buffer",
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//trading:trading_state",
    ],
)

cc_library(
    name = "market_hours",
    srcs = ["market_hours.cc"],
    hdrs = ["market_hours.h"],
    deps = [
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "macd",
    srcs = ["macd.cc"],
    hdrs = ["macd.h"],
    deps = [
        "//containers:vector",
        "//data:aggregate",
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
    ],
)

cc_library(
    name = "noop",
    hdrs = ["noop.h"],
    deps = [
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//trading:trading_state",
    ],
)

cc_library(
    name = "pivot_points",
    srcs = ["pivot_points.cc"],
    hdrs = ["pivot_points.h"],
    deps = [
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_utils",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//trading:trading_state",
    ],
)

cc_test(
    name = "pivot_points_test",
    srcs = ["pivot_points_test.cc"],
    deps = [
        ":pivot_points",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_cc_proto",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//time:conversion",
        "//trading:trading_state",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "profit",
    srcs = ["profit.cc"],
    hdrs = ["profit.h"],
    deps = [
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//trading:pricing",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "vwap",
    srcs = ["vwap.cc"],
    hdrs = ["vwap.h"],
    deps = [
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_utils",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//time:conversion",
        "//time:market_time",
        "//trading:trading_state",
    ],
)

cc_test(
    name = "vwap_test",
    srcs = ["vwap_test.cc"],
    deps = [
        ":vwap",
        "//data:aggregate",
        "//data:analyzer",
        "//data:analyzer_cc_proto",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//time:conversion",
        "//trading:trading_state",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "zig_zag",
    srcs = ["zig_zag.cc"],
    hdrs = ["zig_zag.h"],
    deps = [
        "//containers:vector",
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//trading:trading_state",
    ],
)
