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

cc_library(
    name = "thread_safe",
    hdrs = ["thread_safe.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "thread_safe_test",
    srcs = ["thread_safe_test.cc"],
    deps = [
        ":thread_safe",
        "@googletest//:gtest_main",
    ],
)
