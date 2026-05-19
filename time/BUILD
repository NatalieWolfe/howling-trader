load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "conversion",
    srcs = ["conversion.cc"],
    hdrs = ["conversion.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/time",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:time_util",
        "@protobuf//:timestamp_cc_proto",
    ],
)

cc_test(
    name = "conversion_test",
    srcs = ["conversion_test.cc"],
    deps = [
        ":conversion",
        "@abseil-cpp//absl/time",
        "@googletest//:gtest_main",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:timestamp_cc_proto",
    ],
)
