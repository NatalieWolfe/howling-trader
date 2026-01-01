load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "sqlite_database",
    srcs = ["sqlite_database.cc"],
    hdrs = ["sqlite_database.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//services:database",
        "//services/db/schema",
        "//strings:format",
        "//strings:parse",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@sqlite3",
    ],
)

cc_test(
    name = "sqlite_database_test",
    srcs = ["sqlite_database_test.cc"],
    deps = [
        ":sqlite_database",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "@abseil-cpp//absl/random",
        "@googletest//:gtest_main",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:timestamp_cc_proto",
    ],
)
