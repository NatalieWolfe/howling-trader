load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load", "oci_push")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

cc_library(
    name = "auth_client",
    srcs = ["auth_client.cc"],
    hdrs = ["auth_client.h"],
    deps = [
        "//services/oauth/proto:auth_service_cc_grpc",
        "@grpc//:grpc++",
    ],
)

cc_binary(
    name = "auth_refresh",
    srcs = ["auth_refresh.cc"],
    deps = [
        ":auth_client",
        "//api/schwab:configuration",
        "//environment:init",
        "//services:authenticate",
        "//services:database",
        "//services/db:register",
        "//services/registry",
        "//services/security:register",
        "@abseil-cpp//absl/log",
    ],
)

pkg_tar(
    name = "auth_refresh_package",
    srcs = [":auth_refresh"],
    include_runfiles = True,
    package_dir = "/services/oauth",
)

oci_image(
    name = "auth_refresh_image",
    base = "@ubuntu_noble",
    entrypoint = ["/services/oauth/auth_refresh"],
    env = {
        "RUNFILES_DIR": "/services/oauth/auth_refresh.runfiles",
    },
    tars = [":auth_refresh_package"],
)

oci_load(
    name = "auth_refresh_load",
    image = ":auth_refresh_image",
    repo_tags = ["auth-refresh:latest"],
)

oci_push(
    name = "auth_refresh_push",
    image = ":auth_refresh_image",
    remote_tags = ["latest"],
)

cc_library(
    name = "auth_service",
    srcs = ["auth_service.cc"],
    hdrs = ["auth_service.h"],
    deps = [
        ":notification",
        "//api/schwab:configuration",
        "//api/schwab:oauth",
        "//services:database",
        "//services:security",
        "//services/db/schema:auth_token",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "auth_service_test",
    srcs = ["auth_service_test.cc"],
    deps = [
        ":auth_service",
        ":mock_telegram_server",
        "//services:mock_database",
        "//services:mock_security",
        "//services/db/schema:auth_token",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@abseil-cpp//absl/flags:flag",
        "@googletest//:gtest_main",
        "@grpc//:grpc++",
    ],
)

cc_library(
    name = "mock_auth_service",
    testonly = True,
    hdrs = ["mock_auth_service.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//services/oauth/proto:auth_service_cc_grpc",
        "@googletest//:gtest",
        "@grpc//:grpc++",
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
    name = "mock_telegram_server",
    testonly = True,
    srcs = ["mock_telegram_server.cc"],
    hdrs = ["mock_telegram_server.h"],
    data = [
        "//net:local.wolfe.dev.crt",
        "//net:local.wolfe.dev.key",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//environment:runfiles",
        "@abseil-cpp//absl/flags:flag",
        "@boost.asio",
        "@boost.beast",
    ],
)

cc_library(
    name = "notification",
    srcs = ["notification.cc"],
    hdrs = ["notification.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//net:connect",
        "//net:url",
        "//strings:format",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@boost.asio",
        "@boost.beast",
        "@jsoncpp",
    ],
)

cc_test(
    name = "notification_test",
    srcs = ["notification_test.cc"],
    deps = [
        ":mock_telegram_server",
        ":notification",
        "//strings:json",
        "@abseil-cpp//absl/flags:flag",
        "@googletest//:gtest_main",
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
    name = "oauth_http_service",
    srcs = ["oauth_http_service.cc"],
    hdrs = ["oauth_http_service.h"],
    deps = [
        ":oauth_exchanger",
        ":oauth_exchanger_impl",
        "//api:schwab",
        "//api/schwab:oauth",
        "//net:connect",
        "//services:database",
        "//services:security",
        "@abseil-cpp//absl/log",
        "@boost.asio",
        "@boost.beast",
        "@boost.url",
    ],
)

cc_test(
    name = "oauth_http_service_test",
    srcs = ["oauth_http_service_test.cc"],
    deps = [
        ":mock_auth_service",
        ":mock_oauth_exchanger",
        ":oauth_http_service",
        "//services:authenticate",
        "//services:authenticate_test_utils",
        "//services:mock_database",
        "//services:mock_security",
        "//services:mock_token_refresher",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    deps = [
        ":auth_service",
        ":oauth_exchanger_impl",
        ":oauth_http_service",
        "//api/schwab:connect",
        "//environment:init",
        "//net:connect",
        "//services:database",
        "//services:security",
        "//services/db:register",
        "//services/registry",
        "//services/security:register",
        "@abseil-cpp//absl/log",
        "@grpc//:grpc++",
    ],
)

pkg_tar(
    name = "server_layer",
    srcs = [":server"],
    include_runfiles = True,
    package_dir = "/services/oauth",
)

oci_image(
    name = "image",
    base = "@ubuntu_noble",
    entrypoint = ["/services/oauth/server"],
    env = {
        "RUNFILES_DIR": "/services/oauth/server.runfiles",
    },
    tars = [":server_layer"],
)

oci_load(
    name = "load",
    image = ":image",
    repo_tags = ["howling-oauth:latest"],
)

oci_push(
    name = "push",
    image = ":image",
    remote_tags = ["latest"],
)
