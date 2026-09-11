load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//infrastructure:containers.bzl", "package_binary")

cc_library(
    name = "auth_client",
    srcs = ["auth_client.cc"],
    hdrs = ["auth_client.h"],
    visibility = ["//services:__subpackages__"],
    deps = [
        "//services:security",
        "//services/oauth/proto:auth_service_cc_grpc",
        "//services/registry",
        "@grpc//:grpc++",
    ],
)

cc_test(
    name = "auth_client_test",
    srcs = ["auth_client_test.cc"],
    deps = [
        ":auth_client",
        "//services:mock_security",
        "//services/registry",
        "//services/security:register",
        "@googletest//:gtest_main",
        "@grpc//:grpc++",
    ],
)

cc_test(
    name = "auth_service_integration_test",
    srcs = ["auth_service_integration_test.cc"],
    data = [
        "//net:local.wolfe.dev.crt",
        "//net:local.wolfe.dev.key",
    ],
    deps = [
        ":auth_service",
        ":mock_telegram_server",
        "//environment:runfiles",
        "//files",
        "//services:mock_database",
        "//services/db/schema:auth_token",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@googletest//:gtest_main",
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

package_binary(
    name = "auth_refresh_image",
    image_name = "howling-auth-refresh",
    binary = ":auth_refresh",
    visibility = ["//infrastructure:visibility"],
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
    name = "errors",
    hdrs = ["errors.h"],
    visibility = ["//visibility:public"],
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
        "//services/db/schema:auth_token",
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
        "//services/security:certificate_bundle",
        "//services/security:register",
        "@abseil-cpp//absl/log",
        "@grpc//:grpc++",
    ],
)

package_binary(
    name = "server_image",
    image_name = "howling-auth-service",
    binary = ":server",
    visibility = ["//infrastructure:visibility"],
)
