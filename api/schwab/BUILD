load("@rules_cc//cc:defs.bzl", "cc_library")

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
        "@abseil-cpp//absl/flags:flag",
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
        "//net:connect",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@boost.beast",
        "@boost.url",
        "@jsoncpp",
    ],
)
