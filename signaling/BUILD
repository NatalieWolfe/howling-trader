load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "cancellation",
    srcs = ["cancellation.cc"],
    hdrs = ["cancellation.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":signal_handler",
        "@abseil-cpp//absl/log",
    ],
)

cc_test(
    name = "cancellation_test",
    srcs = ["cancellation_test.cc"],
    deps = [
        ":cancellation",
        ":signal_handler",
        "//logs/testing:log_capture",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "signal_handler",
    srcs = ["signal_handler.cc"],
    hdrs = ["signal_handler.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "signal_handler_test",
    srcs = ["signal_handler_test.cc"],
    deps = [
        ":signal_handler",
        "@googletest//:gtest_main",
    ],
)
