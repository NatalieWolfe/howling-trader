load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//api:__subpackages__"])

cc_library(
    name = "connect",
    srcs = ["connect.cc"],
    hdrs = ["connect.h"],
    visibility = [
        "//api:__subpackages__",
        "//services:__pkg__",
        "//services/oauth:__pkg__",
    ],
    deps = [
        "//net:connect",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "configuration",
    srcs = ["configuration.cc"],
    hdrs = ["configuration.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//services:security",
        "//services/registry",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@jsoncpp",
    ],
)

cc_library(
    name = "oauth",
    srcs = ["oauth.cc"],
    hdrs = ["oauth.h"],
    visibility = [
        "//api:__subpackages__",
        "//services:__pkg__",
        "//services/oauth:__pkg__",
    ],
    deps = [
        ":configuration",
        ":connect",
        ":utils",
        "//net:connect",
        "//services/oauth:errors",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@boost.beast",
        "@boost.url",
        "@jsoncpp",
    ],
)

cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    visibility = [
        "//api:__pkg__",
        "//api:__subpackages__",
    ],
    deps = [
        "//net:gzip",
        "@boost.beast",
    ],
)

cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":utils",
        "//net:gzip",
        "@boost.beast",
        "@googletest//:gtest_main",
    ],
)
