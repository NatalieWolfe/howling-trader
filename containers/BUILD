load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "buffered_stream",
    hdrs = ["buffered_stream.h"],
    visibility = ["//visibility:public"],
    deps = [":circular_buffer"],
)

cc_test(
    name = "buffered_stream_test",
    size = "small",
    srcs = ["buffered_stream_test.cc"],
    deps = [
        ":buffered_stream",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "circular_buffer",
    hdrs = ["circular_buffer.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "circular_buffer_test",
    size = "small",
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
