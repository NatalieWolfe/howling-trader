load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

filegroup(
    name = "history",
    data = glob(["history/**/*.textproto"]),
    visibility = ["//visibility:public"],
)

proto_library(
    name = "account_proto",
    srcs = ["account.proto"],
)

cc_proto_library(
    name = "account_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":account_proto"],
)

cc_library(
    name = "aggregate",
    srcs = ["aggregate.cc"],
    hdrs = ["aggregate.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":candle_cc_proto",
        "//containers:vector",
        "//time:conversion",
        "//time:market_time",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_test(
    name = "aggregate_test",
    srcs = ["aggregate_test.cc"],
    deps = [
        ":aggregate",
        ":candle_cc_proto",
        "//time:conversion",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "analyzer",
    srcs = ["analyzer.cc"],
    hdrs = ["analyzer.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":stock_cc_proto",
        "//trading:trading_state",
    ],
)

proto_library(
    name = "analyzer_proto",
    srcs = ["analyzer.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@protobuf//:duration_proto",
        "@protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "analyzer_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":analyzer_proto"],
)

cc_library(
    name = "analyzer_utils",
    srcs = ["analyzer_utils.cc"],
    hdrs = ["analyzer_utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":aggregate",
        ":analyzer_cc_proto",
        ":stock_cc_proto",
        "//containers:vector",
        "//trading:trading_state",
    ],
)

cc_test(
    name = "analyzer_utils_test",
    srcs = ["analyzer_utils_test.cc"],
    deps = [
        ":aggregate",
        ":analyzer_cc_proto",
        ":analyzer_utils",
        ":candle_cc_proto",
        ":stock_cc_proto",
        "//containers:vector",
        "//trading:trading_state",
        "@googletest//:gtest_main",
    ],
)

proto_library(
    name = "candle_proto",
    srcs = ["candle.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@protobuf//:duration_proto",
        "@protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "candle_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":candle_proto"],
)

cc_library(
    name = "load_analyzer",
    srcs = ["load_analyzer.cc"],
    hdrs = ["load_analyzer.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":aggregate",
        ":analyzer",
        ":analyzer_cc_proto",
        ":stock_cc_proto",
        "//data/analyzers:bollinger",
        "//data/analyzers:howling",
        "//data/analyzers:macd",
        "//data/analyzers:market_hours",
        "//data/analyzers:noop",
        "//data/analyzers:pivot_points",
        "//data/analyzers:profit",
        "//data/analyzers:zig_zag",
    ],
)

proto_library(
    name = "market_proto",
    srcs = ["market.proto"],
    visibility = ["//visibility:public"],
    deps = [
        ":stock_proto",
        "@protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "market_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":market_proto"],
)

proto_library(
    name = "stock_proto",
    srcs = ["stock.proto"],
    visibility = ["//visibility:public"],
    deps = [
        ":candle_proto",
        "@protobuf//:duration_proto",
        "@protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "stock_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":stock_proto"],
)

proto_library(
    name = "trade_proto",
    srcs = ["trade.proto"],
    visibility = ["//visibility:public"],
    deps = [
        ":stock_proto",
        "@protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "trade_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":trade_proto"],
)

cc_library(
    name = "utilities",
    srcs = ["utilities.cc"],
    hdrs = ["utilities.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":stock_cc_proto",
        "//files",
        "//strings:format",
        "@abseil-cpp//absl/strings",
        "@protobuf",
    ],
)

cc_library(
    name = "wilder_rma",
    srcs = ["wilder_rma.cc"],
    hdrs = ["wilder_rma.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "wilder_rma_test",
    srcs = ["wilder_rma_test.cc"],
    deps = [
        ":wilder_rma",
        "@googletest//:gtest_main",
    ],
)
