load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "alpaca",
    srcs = ["alpaca.cc"],
    hdrs = ["alpaca.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//containers:vector",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//net:connect",
        "//net:url",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@boost.beast",
        "@boost.url",
        "@jsoncpp",
        "@protobuf//:time_util",
    ],
)

cc_library(
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    visibility = ["//api:__subpackages__"],  # Move before growing visibility.
    deps = ["@jsoncpp"],
)

cc_library(
    name = "schwab",
    srcs = ["schwab.cc"],
    hdrs = ["schwab.h"],
    data = [
        "//net:local.wolfe.dev.crt",
        "//net:local.wolfe.dev.key",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":json",
        "//api/schwab:connect",
        "//api/schwab:oauth",
        "//containers:vector",
        "//data:account_cc_proto",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//net:connect",
        "//net:url",
        "//strings:parse",
        "//time:conversion",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@boost.beast",
        "@boost.url",
        "@jsoncpp",
        "@protobuf//:time_util",
    ],
)
