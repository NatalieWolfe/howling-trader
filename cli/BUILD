load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "colorize",
    srcs = ["colorize.cc"],
    hdrs = ["colorize.h"],
    visibility = ["//visibility:public"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_library(
    name = "printing",
    srcs = ["printing.cc"],
    hdrs = ["printing.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":colorize",
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//time:conversion",
        "//trading:metrics",
        "//trading:trading_state",
        "@abseil-cpp//absl/strings",
    ],
)
