load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_binary(
    name = "evaluate",
    srcs = ["evaluate.cc"],
    data = ["//data:history"],
    deps = [
        ":init",
        ":runfiles",
        "//cli:printing",
        "//containers:vector",
        "//data:aggregate",
        "//data:analyzer",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//time:conversion",
        "//trading:metrics",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_binary(
    name = "execute",
    srcs = ["execute.cc"],
    deps = [
        ":init",
        "//api:schwab",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//time:conversion",
    ],
)

cc_binary(
    name = "fetch",
    srcs = ["fetch.cc"],
    deps = [
        ":init",
        "//api:alpaca",
        "//api:schwab",
        "//containers:vector",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//strings:format",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@protobuf",
    ],
)

cc_library(
    name = "init",
    srcs = ["init.cc"],
    hdrs = ["init.h"],
    deps = [
        ":runfiles_init",
        "//logs:stdout_sink",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log:flags",
        "@abseil-cpp//absl/log:initialize",
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

cc_binary(
    name = "visualize",
    srcs = ["visualize.cc"],
    data = ["//data:history"],
    deps = [
        ":init",
        ":runfiles",
        "//cli:printing",
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//time:conversion",
        "//trading:metrics",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)
