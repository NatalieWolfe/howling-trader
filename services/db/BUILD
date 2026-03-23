load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load", "oci_push")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@rules_shell//shell:sh_test.bzl", "sh_test")

cc_library(
    name = "database_test_interface",
    testonly = True,
    hdrs = ["database_test_interface.h"],
    deps = [
        ":environment",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services:database",
        "//services:mock_security",
        "@googletest//:gtest",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:timestamp_cc_proto",
    ],
)

cc_library(
    name = "environment",
    srcs = ["environment.cc"],
    hdrs = ["environment.h"],
    deps = ["@abseil-cpp//absl/flags:flag"],
)

cc_library(
    name = "postgres_database",
    srcs = ["postgres_database.cc"],
    hdrs = ["postgres_database.h"],
    deps = [
        ":environment",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//services:database",
        "//services:security",
        "//services/db/schema",
        "//services/db/schema:auth_token",
        "//time:conversion",
        "@abseil-cpp//absl/log",
        "@postgres//:libpq",
    ],
)

cc_binary(
    name = "postgres_database_test_bin",
    testonly = True,
    srcs = ["postgres_database_test.cc"],
    deps = [
        ":database_test_interface",
        ":environment",
        ":postgres_database",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services:mock_security",
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
    name = "register",
    srcs = ["register.cc"],
    hdrs = ["register.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":postgres_database",
        ":sqlite_database",
        "//services:database",
        "//services:security",
        "//services/registry:register_service",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
    ],
)

cc_binary(
    name = "schema_upgrade",
    srcs = ["schema_upgrade.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":register",
        "//environment:init",
        "//services:database",
        "//services/registry",
        "//services/security:register",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
    ],
)

cc_library(
    name = "sqlite_database",
    srcs = ["sqlite_database.cc"],
    hdrs = ["sqlite_database.h"],
    deps = [
        ":environment",
        "//data:candle_cc_proto",
        "//data:market_cc_proto",
        "//data:stock_cc_proto",
        "//services:database",
        "//services:security",
        "//services/db/schema",
        "//services/db/schema:auth_token",
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
        ":environment",
        ":sqlite_database",
        "//data:candle_cc_proto",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services:mock_security",
        "//time:conversion",
        "@abseil-cpp//absl/random",
        "@googletest//:gtest_main",
        "@protobuf//:duration_cc_proto",
        "@protobuf//:timestamp_cc_proto",
        "@sqlite3",
    ],
)

pkg_tar(
    name = "schema_upgrade_layer",
    srcs = [":schema_upgrade"],
    include_runfiles = True,
    package_dir = "/services/db",
)

oci_image(
    name = "image",
    base = "@ubuntu_noble",
    entrypoint = ["/services/db/schema_upgrade"],
    env = {
        "RUNFILES_DIR": "/services/db/schema_upgrade.runfiles",
    },
    tars = [":schema_upgrade_layer"],
)

oci_load(
    name = "load",
    image = ":image",
    repo_tags = ["schema-upgrade:latest"],
)

oci_push(
    name = "push",
    image = ":image",
    remote_tags = ["latest"],
)
