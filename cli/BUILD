load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "colorize",
    srcs = ["colorize.cc"],
    hdrs = ["colorize.h"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_library(
    name = "printing",
    srcs = ["printing.cc"],
    hdrs = ["printing.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":colorize",
        "//data:candle_cc_proto",
        "@abseil-cpp//absl/strings",
    ],
)
