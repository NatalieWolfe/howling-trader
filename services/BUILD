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
        "@abseil-cpp//absl/log",
        "@grpc//:grpc++",
    ],
)

cc_test(
    name = "authenticate_test",
    srcs = ["authenticate_test.cc"],
    deps = [
        ":authenticate",
        ":mock_database",
        "//services/oauth:mock_auth_service",
        "//services/oauth/proto:auth_service_cc_grpc",
        "@googletest//:gtest_main",
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
