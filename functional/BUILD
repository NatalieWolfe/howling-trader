load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "retry",
    hdrs = ["retry.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "retry_test",
    size = "small",
    srcs = ["retry_test.cc"],
    deps = [
        ":retry",
        "@googletest//:gtest_main",
    ],
)
