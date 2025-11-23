load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "stdout_sink",
    srcs = ["stdout_sink.cc"],
    hdrs = ["stdout_sink.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/log:log_entry",
        "@abseil-cpp//absl/log:log_sink",
    ],
)
