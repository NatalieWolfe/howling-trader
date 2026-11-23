load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "log_capture",
    testonly = True,
    srcs = ["log_capture.cc"],
    hdrs = ["log_capture.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/base:log_severity",
        "@abseil-cpp//absl/log:log_entry",
        "@abseil-cpp//absl/log:log_sink",
        "@abseil-cpp//absl/log:log_sink_registry",
    ],
)

cc_test(
    name = "log_capture_test",
    srcs = ["log_capture_test.cc"],
    deps = [
        ":log_capture",
        "@abseil-cpp//absl/base:log_severity",
        "@abseil-cpp//absl/log",
        "@googletest//:gtest_main",
    ],
)
