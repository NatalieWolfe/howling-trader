load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_shell//shell:sh_test.bzl", "sh_test")

cc_library(
    name = "crypto",
    srcs = ["crypto.cc"],
    hdrs = ["crypto.h"],
    deps = [
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
        "@boringssl//:crypto",
    ],
)

cc_test(
    name = "crypto_test",
    srcs = ["crypto_test.cc"],
    deps = [
        ":crypto",
        "@abseil-cpp//absl/flags:flag",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "make_database",
    srcs = ["make_database.cc"],
    hdrs = ["make_database.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":postgres_database",
        ":sqlite_database",
        "//services:database",
        "@abseil-cpp//absl/flags:flag",
    ],
)

cc_library(
    name = "postgres_database",
    srcs = ["postgres_database.cc"],
    hdrs = ["postgres_database.h"],
    deps = [
        ":crypto",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//services:database",
        "//services/db/schema",
        "//time:conversion",
        "@abseil-cpp//absl/log",
        "@postgres//:libpq",
    ],
)

cc_library(
    name = "database_test_interface",
    testonly = True,
    hdrs = ["database_test_interface.h"],
    deps = [
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "@googletest//:gtest",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:timestamp_cc_proto",
    ],
)

cc_binary(
    name = "postgres_database_test_bin",
    testonly = True,
    srcs = ["postgres_database_test.cc"],
    deps = [
        ":database_test_interface",
        ":postgres_database",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/random",
        "@googletest//:gtest",
        "@postgres//:libpq",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:timestamp_cc_proto",
    ],
)

sh_test(
    name = "postgres_database_test",
    srcs = ["run_postgres_database_test.sh"],
    args = ["$(location :postgres_database_test_bin)"],
    data = [":postgres_database_test_bin"],
    tags = [
        "no-remote",
        "requires-network",
    ],
)

cc_library(
    name = "sqlite_database",
    srcs = ["sqlite_database.cc"],
    hdrs = ["sqlite_database.h"],
    deps = [
        ":crypto",
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
        ":database_test_interface",
        ":sqlite_database",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//time:conversion",
        "@abseil-cpp//absl/random",
        "@googletest//:gtest_main",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:timestamp_cc_proto",
        "@sqlite3",
    ],
)
