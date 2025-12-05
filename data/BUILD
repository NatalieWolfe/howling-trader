load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library")

filegroup(
    name = "history",
    data = glob(["history/**/*.textproto"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "aggregate",
    srcs = ["aggregate.cc"],
    hdrs = ["aggregate.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":candle_cc_proto",
        "//containers:vector",
        "@abseil-cpp//absl/flags:flag",
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
    name = "candle_proto",
    srcs = ["candle.proto"],
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
        ":stock_cc_proto",
        "//data/analyzers:bollinger",
        "//data/analyzers:howling",
        "//data/analyzers:macd",
        "//data/analyzers:market_hours",
        "//data/analyzers:noop",
        "//data/analyzers:profit",
        "//data/analyzers:zig_zag",
    ],
)

proto_library(
    name = "market_proto",
    srcs = ["market.proto"],
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

cc_library(
    name = "utilities",
    srcs = ["utilities.cc"],
    hdrs = ["utilities.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":stock_cc_proto",
        "//strings:format",
        "@abseil-cpp//absl/strings",
        "@protobuf",
    ],
)
