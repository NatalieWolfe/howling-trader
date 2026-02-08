load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "configuration",
    srcs = ["configuration.cc"],
    hdrs = ["configuration.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "init",
    srcs = ["init.cc"],
    hdrs = ["init.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":runfiles_init",
        "//logs:json_sink",
        "//logs:stdout_sink",
        "@abseil-cpp//absl/base:log_severity",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log:globals",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/log:log_sink_registry",
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
    visibility = ["//visibility:public"],
    deps = [":runfiles"],
)
