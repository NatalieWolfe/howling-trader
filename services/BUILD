load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "authenticate",
    srcs = ["authenticate.cc"],
    hdrs = ["authenticate.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//api/schwab:connect",
        "//api/schwab:oauth",
        "//net:connect",
        "//services:database",
        "//services/db/schema:auth_token",
        "//services/oauth/proto:auth_service_cc_grpc",
        "//services/registry",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/time",
        "@grpc//:grpc++",
    ],
)

cc_test(
    name = "authenticate_test",
    srcs = ["authenticate_test.cc"],
    deps = [
        ":authenticate",
        ":mock_database",
        ":mock_security",
        ":mock_token_refresher",
        "//services/db/schema:auth_token",
        "//services/oauth:mock_auth_service",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/time",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "authenticate_test_utils",
    testonly = True,
    hdrs = ["authenticate_test_utils.h"],
    visibility = ["//visibility:public"],
    deps = [":authenticate"],
)

cc_library(
    name = "mock_token_refresher",
    testonly = True,
    hdrs = ["mock_token_refresher.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":authenticate",
        "//api/schwab:oauth",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "database",
    hdrs = ["database.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":service_base",
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services/db/schema:auth_token",
    ],
)

cc_library(
    name = "mock_database",
    testonly = True,
    hdrs = ["mock_database.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":database",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "market_watch",
    srcs = ["market_watch.cc"],
    hdrs = ["market_watch.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//api:schwab",
        "//containers:buffered_stream",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "security",
    hdrs = ["security.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":service_base",
        "@jsoncpp",
    ],
)

cc_library(
    name = "mock_security",
    testonly = True,
    hdrs = ["mock_security.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":security",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "service_base",
    hdrs = ["service_base.h"],
    visibility = ["//services:__subpackages__"],
)
