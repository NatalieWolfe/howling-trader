load("@rules_cc//cc:defs.bzl", "cc_library")

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
