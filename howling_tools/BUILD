load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_binary(
    name = "fetch",
    srcs = ["fetch.cc"],
    deps = [
        ":runfiles_init",
        "//api:alpaca",
        "//api:schwab",
        "//containers:vector",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//logs:stdout_sink",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log:flags",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/strings",
        "@protobuf",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:time_util",
        "@protobuf//:timestamp_cc_proto",
    ],
)

cc_library(
    name = "runfiles",
    srcs = ["runfiles.cc"],
    hdrs = ["runfiles.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/log",
        "@bazel_tools//tools/cpp/runfiles",
    ],
)

cc_library(
    name = "runfiles_init",
    hdrs = ["runfiles_init.h"],
    deps = [":runfiles"],
)
