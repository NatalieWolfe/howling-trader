load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

cc_library(
    name = "auth_service_lib",
    srcs = ["auth_service.cc"],
    hdrs = ["auth_service.h"],
    deps = [
        "//api/schwab:configuration",
        "//api/schwab:oauth",
        "//services:database",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
    ],
)

cc_library(
    name = "oauth_exchanger",
    hdrs = ["oauth_exchanger.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//api/schwab:oauth",
        "//net:connect",
    ],
)

cc_library(
    name = "oauth_exchanger_impl",
    srcs = ["oauth_exchanger_impl.cc"],
    hdrs = ["oauth_exchanger_impl.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":oauth_exchanger",
        "//api/schwab:oauth",
        "//net:connect",
    ],
)

cc_library(
    name = "mock_oauth_exchanger",
    testonly = True,
    hdrs = ["mock_oauth_exchanger.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":oauth_exchanger",
        "//api/schwab:oauth",
        "//net:connect",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "oauth_http_service_lib",
    srcs = ["oauth_http_service.cc"],
    hdrs = ["oauth_http_service.h"],
    deps = [
        ":oauth_exchanger",
        ":oauth_exchanger_impl",
        "//api/schwab:oauth",
        "//net:connect",
        "//services:database",
        "@abseil-cpp//absl/log",
        "@boost.asio",
        "@boost.beast",
        "@boost.url",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    deps = [
        ":auth_service_lib",
        ":oauth_exchanger_impl",
        ":oauth_http_service_lib",
        "//api/schwab:connect",
        "//environment:init",
        "//net:connect",
        "//services/db:make_database",
        "@abseil-cpp//absl/log",
        "@grpc//:grpc++",
    ],
)

pkg_tar(
    name = "server_layer",
    srcs = [":server"],
)

oci_image(
    name = "image",
    base = "@distroless_cc",
    entrypoint = ["/services/oauth/server"],
    tars = [":server_layer"],
)

oci_load(
    name = "load",
    image = ":image",
    repo_tags = ["howling-oauth:latest"],
)

cc_test(
    name = "auth_service_test",
    srcs = ["auth_service_test.cc"],
    deps = [
        ":auth_service_lib",
        "//services:mock_database",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@googletest//:gtest_main",
        "@grpc//:grpc++",
    ],
)

cc_test(
    name = "oauth_http_service_test",
    srcs = ["oauth_http_service_test.cc"],
    deps = [
        ":mock_oauth_exchanger",
        ":oauth_http_service_lib",
        "//services:mock_database",
        "@googletest//:gtest_main",
    ],
)
