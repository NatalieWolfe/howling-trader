load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load", "oci_push")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

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
        "//services/db:make_database",
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
        "//services:database",
        "//services:market_watch",
        "//services/db:make_database",
        "//time:conversion",
        "//trading:executor",
        "//trading:trading_state",
        "@abseil-cpp//absl/flags:flag",
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

pkg_tar(
    name = "execute_layer",
    srcs = [":execute"],
)

oci_image(
    name = "executor_image",
    base = "@distroless_cc",
    entrypoint = ["/howling_tools/execute"],
    tars = [":execute_layer"],
)

# For local testing: bazel run //howling_tools:load
oci_load(
    name = "load",
    image = ":executor_image",
    repo_tags = ["howling-trader:latest"],
)

# For pushing: bazel run //howling_tools:push
oci_push(
    name = "push",
    image = ":executor_image",
    remote_tags = ["latest"],
    repository = "ghcr.io/nataliewolfe/howling-trader",
)
