load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//infrastructure:containers.bzl", "package_binary")

cc_library(
    name = "service",
    srcs = ["service.cc"],
    hdrs = ["service.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//services/db/reader:reader_db",
        "//services/mcp:dispatcher",
        "//services/reader_mcp/handlers:actions_resource_handler",
        "//services/reader_mcp/handlers:candles_handler",
        "//services/reader_mcp/handlers:market_data_handler",
        "//services/reader_mcp/handlers:schema_resource_handler",
        "//services/reader_mcp/handlers:symbols_resource_handler",
        "//services/reader_mcp/handlers:trades_handler",
        "//services/reader_mcp/handlers:trading_summary_handler",
    ],
)

cc_test(
    name = "service_test",
    srcs = ["service_test.cc"],
    deps = [
        ":service",
        "//data:stock_cc_proto",
        "//services/db/reader:mock_reader_db",
        "//services/db/reader:reader_db",
        "//services/mcp:dispatcher",
        "//strings:json",
        "@googletest//:gtest_main",
        "@jsoncpp",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    deps = [
        ":service",
        "//environment:init",
        "//services/db:environment",
        "//services/db/reader:reader_db",
        "//services/db/reader:register",
        "//services/mcp:http_server",
        "//services/registry",
        "//signaling:signal_handler",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log",
        "@boost.asio",
    ],
)

package_binary(
    name = "server_image",
    binary = ":server",
    image_name = "howling-reader-mcp",
    visibility = ["//infrastructure:visibility"],
)
