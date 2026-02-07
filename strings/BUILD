load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//:__subpackages__"])

cc_library(
    name = "format",
    srcs = ["format.cc"],
    hdrs = ["format.h"],
)

cc_library(
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    visibility = ["//visibility:public"],
    deps = ["@jsoncpp"],
)

cc_test(
    name = "json_test",
    srcs = ["json_test.cc"],
    deps = [
        ":json",
        "@googletest//:gtest_main",
        "@jsoncpp",
    ],
)

cc_library(
    name = "parse",
    srcs = ["parse.cc"],
    hdrs = ["parse.h"],
)

cc_test(
    name = "parse_test",
    srcs = ["parse_test.cc"],
    deps = [
        ":parse",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "trim",
    srcs = ["trim.cc"],
    hdrs = ["trim.h"],
)

cc_test(
    name = "trim_test",
    srcs = ["trim_test.cc"],
    deps = [
        ":trim",
        "@googletest//:gtest_main",
    ],
)
