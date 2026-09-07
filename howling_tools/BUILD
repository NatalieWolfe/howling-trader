load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//infrastructure:containers.bzl", "package_binary")

cc_binary(
    name = "evaluate",
    srcs = ["evaluate.cc"],
    data = ["//data:history"],
    deps = [
        "//cli:printing",
        "//containers:vector",
        "//data:aggregate",
        "//data:analyzer",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:init",
        "//environment:runfiles",
        "//services:database",
        "//services:security",
        "//services/db:register",
        "//services/registry",
        "//services/security:register",
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
        "//api:schwab",
        "//cli:printing",
        "//containers:vector",
        "//data:account_cc_proto",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:configuration",
        "//environment:init",
        "//files",
        "//services:database",
        "//services:market_watch",
        "//services:security",
        "//services/db:register",
        "//services/registry",
        "//services/security:register",
        "//strings:format",
        "//time:conversion",
        "//trading:executor",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
    ],
)

cc_binary(
    name = "fetch",
    srcs = ["fetch.cc"],
    deps = [
        "//api:alpaca",
        "//api:schwab",
        "//containers:vector",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:init",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@protobuf",
    ],
)

cc_binary(
    name = "visualize",
    srcs = ["visualize.cc"],
    data = ["//data:history"],
    deps = [
        "//cli:printing",
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//data:load_analyzer",
        "//data:stock_cc_proto",
        "//data:utilities",
        "//environment:init",
        "//environment:runfiles",
        "//time:conversion",
        "//trading:metrics",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
    ],
)

package_binary(
    name = "execute_image",
    image_name = "howling-execute",
    binary = ":execute",
    visibility = ["//infrastructure:visibility"],
)
