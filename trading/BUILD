load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "pricing",
    srcs = ["pricing.cc"],
    hdrs = ["pricing.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:trading_state",
    ],
)
