load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//services/reader_mcp:__subpackages__"])

cc_library(
    name = "validation",
    srcs = ["validation.cc"],
    hdrs = ["validation.h"],
    deps = [
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services/db/reader:reader_db",
        "//services/mcp:tool_handler",
        "@abseil-cpp//absl/time",
        "@jsoncpp",
    ],
)

cc_library(
    name = "candles_handler",
    srcs = ["candles_handler.cc"],
    hdrs = ["candles_handler.h"],
    deps = [
        ":validation",
        "//services/db/reader:reader_db",
        "//services/mcp:tool_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_library(
    name = "market_data_handler",
    srcs = ["market_data_handler.cc"],
    hdrs = ["market_data_handler.h"],
    deps = [
        ":validation",
        "//services/db/reader:reader_db",
        "//services/mcp:tool_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_library(
    name = "trades_handler",
    srcs = ["trades_handler.cc"],
    hdrs = ["trades_handler.h"],
    deps = [
        ":validation",
        "//services/db/reader:reader_db",
        "//services/mcp:tool_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_library(
    name = "trading_summary_handler",
    srcs = ["trading_summary_handler.cc"],
    hdrs = ["trading_summary_handler.h"],
    deps = [
        ":validation",
        "//services/db/reader:reader_db",
        "//services/mcp:tool_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_library(
    name = "symbols_resource_handler",
    srcs = ["symbols_resource_handler.cc"],
    hdrs = ["symbols_resource_handler.h"],
    deps = [
        ":validation",
        "//services/db/reader:reader_db",
        "//services/mcp:resource_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_library(
    name = "actions_resource_handler",
    srcs = ["actions_resource_handler.cc"],
    hdrs = ["actions_resource_handler.h"],
    deps = [
        "//services/db/reader:reader_db",
        "//services/mcp:resource_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_library(
    name = "schema_resource_handler",
    srcs = ["schema_resource_handler.cc"],
    hdrs = ["schema_resource_handler.h"],
    deps = [
        "//services/db/reader:reader_db",
        "//services/mcp:resource_handler",
        "//strings:json",
        "@jsoncpp",
    ],
)

cc_test(
    name = "handlers_test",
    srcs = ["handlers_test.cc"],
    deps = [
        ":actions_resource_handler",
        ":candles_handler",
        ":market_data_handler",
        ":schema_resource_handler",
        ":symbols_resource_handler",
        ":trades_handler",
        ":trading_summary_handler",
        ":validation",
        "//data:stock_cc_proto",
        "//data:trade_cc_proto",
        "//services/db/reader:mock_reader_db",
        "//services/db/reader:reader_db",
        "//services/mcp:tool_handler",
        "//strings:json",
        "@googletest//:gtest_main",
        "@jsoncpp",
    ],
)
