load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "circular_buffer",
    hdrs = ["circular_buffer.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "circular_buffer_test",
    srcs = ["circular_buffer_test.cc"],
    deps = [
        ":circular_buffer",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "vector",
    hdrs = ["vector.h"],
    visibility = ["//visibility:public"],
)
