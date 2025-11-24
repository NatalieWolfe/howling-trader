load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "bollinger",
    srcs = ["bollinger.cc"],
    hdrs = ["bollinger.h"],
    deps = [
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//data:trading_state",
    ],
)

cc_library(
    name = "market_hours",
    srcs = ["market_hours.cc"],
    hdrs = ["market_hours.h"],
    deps = [
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//data:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "noop",
    hdrs = ["noop.h"],
    deps = [
        "//data:analyzer",
        "//data:stock_cc_proto",
        "//data:trading_state",
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
        "//data:trading_state",
    ],
)
