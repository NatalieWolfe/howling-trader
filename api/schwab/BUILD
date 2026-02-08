load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//api:__subpackages__"])

cc_library(
    name = "connect",
    srcs = ["connect.cc"],
    hdrs = ["connect.h"],
    deps = [
        "//net:connect",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "oauth_internal",
    srcs = ["oauth_internal.cc"],
    hdrs = ["oauth_internal.h"],
    deps = [
        ":connect",
        "//environment:runfiles",
        "//net:connect",
        "//strings:json",
        "//strings:trim",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@boost.beast",
        "@boost.url",
        "@jsoncpp",
    ],
)
