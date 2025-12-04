load("@rules_cc//cc:defs.bzl", "cc_library")

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
    ],
)
