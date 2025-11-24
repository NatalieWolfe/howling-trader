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
    ],
)

cc_library(
    name = "analyzer",
    hdrs = ["analyzer.h"],
    visibility = ["//visibility:public"],
    deps = [":aggregate"],
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
    name = "trading_state",
    srcs = ["trading_state.cc"],
    hdrs = ["trading_state.h"],
    deps = [
        ":aggregate",
        ":stock_cc_proto",
        "//containers:vector",
    ],
)
