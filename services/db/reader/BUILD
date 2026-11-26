load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_shell//shell:sh_test.bzl", "sh_test")

cc_library(
    name = "reader_db",
    hdrs = ["reader_db.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//containers:resource_pool",
        "//services:service_base",
    ],
)

cc_library(
    name = "mock_reader_db",
    testonly = True,
    hdrs = ["mock_reader_db.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":reader_db",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "sql_builder",
    srcs = ["sql_builder.cc"],
    hdrs = ["sql_builder.h"],
    deps = [
        ":reader_db",
        "//data:trade_cc_proto",
        "//services/db/pg:query",
    ],
)

cc_library(
    name = "postgres_reader_db",
    srcs = ["postgres_reader_db.cc"],
    hdrs = ["postgres_reader_db.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":reader_db",
        ":sql_builder",
        "//services/db/pg:connection",
        "//services/db/pg:query",
        "@postgres//:libpq",
    ],
)

cc_library(
    name = "register",
    srcs = ["register.cc"],
    hdrs = ["register.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":postgres_reader_db",
        ":reader_db",
        "//services/db:environment",
        "//services/db/pg:connection",
        "//services/registry:register_service",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
    ],
)

cc_test(
    name = "reader_db_test",
    srcs = ["reader_db_test.cc"],
    deps = [
        ":mock_reader_db",
        ":reader_db",
        ":sql_builder",
        "//services/db/pg:types",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
    name = "postgres_reader_db_test_bin",
    testonly = True,
    srcs = ["postgres_reader_db_test.cc"],
    deps = [
        ":postgres_reader_db",
        ":reader_db",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services:mock_security",
        "//services/db:environment",
        "//services/db:postgres_database",
        "//services/db/pg:connection",
        "//services/db/pg:query",
        "//services/db/schema",
        "//time:conversion",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@googletest//:gtest",
        "@postgres//:libpq",
    ],
)

sh_test(
    name = "postgres_reader_db_test",
    srcs = ["//services/db:run_postgres_database_test.sh"],
    args = ["$(location :postgres_reader_db_test_bin)"],
    data = [":postgres_reader_db_test_bin"],
    tags = [
        "local",
        "no-remote",
        "requires-network",
    ],
)
