load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "connect",
    srcs = ["connect.cc"],
    hdrs = ["connect.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":url",
        "@boost.asio",
        "@boost.beast",
    ],
)

exports_files(
    [
        "local.wolfe.dev.crt",
        "local.wolfe.dev.key",
    ],
    visibility = ["//api:__subpackages__"],
)

cc_library(
    name = "url",
    hdrs = ["url.h"],
    visibility = ["//visibility:public"],
)
