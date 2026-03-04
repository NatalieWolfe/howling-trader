load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "files",
    srcs = ["files.cc"],
    hdrs = ["files.h"],
    visibility = ["//visibility:public"],
    deps = ["@abseil-cpp//absl/strings"],
)

cc_test(
    name = "files_test",
    srcs = ["files_test.cc"],
    deps = [
        ":files",
        "@googletest//:gtest_main",
    ],
)
