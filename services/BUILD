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
        "//services/db:make_database",
        "//services/oauth/proto:auth_service_cc_grpc",
        "//services/security:bao_client",
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
        "//data:analyzer",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
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
    deps = ["@jsoncpp"],
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
