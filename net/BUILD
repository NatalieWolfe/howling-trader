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
    visibility = ["//visibility:public"],
)

cc_library(
    name = "url",
    hdrs = ["url.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "request",
    srcs = ["request.cc"],
    hdrs = ["request.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":connect",
        "//strings:json",
        "@boost.beast",
        "@jsoncpp",
    ],
)
