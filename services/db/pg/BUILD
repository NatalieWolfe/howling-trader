load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "types",
    srcs = ["types.cc"],
    hdrs = ["types.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "connection",
    srcs = ["connection.cc"],
    hdrs = ["connection.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/log",
        "@postgres//:libpq",
    ],
)

cc_library(
    name = "query",
    srcs = ["query.cc"],
    hdrs = ["query.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":connection",
        ":types",
        "@postgres//:libpq",
    ],
)

cc_test(
    name = "query_test",
    srcs = ["query_test.cc"],
    deps = [
        ":connection",
        ":query",
        ":types",
        "@googletest//:gtest_main",
    ],
)
