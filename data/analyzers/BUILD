load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "noop",
    hdrs = ["noop.h"],
    deps = [
        "//data:aggregate",
        "//data:analyzer",
    ],
)

cc_library(
    name = "zig_zag",
    srcs = ["zig_zag.cc"],
    hdrs = ["zig_zag.h"],
    deps = [
        "//containers:vector",
        "//data:aggregate",
        "//data:analyzer",
        "//data:candle_cc_proto",
    ],
)
