load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

filegroup(
    name = "full_schema",
    srcs = ["full.sql"],
)

filegroup(
    name = "updates",
    srcs = glob(
        ["update_*.sql"],
        allow_empty = True,
    ),
)

cc_library(
    name = "schema",
    srcs = ["schema.cc"],
    hdrs = ["schema.h"],
    data = [
        ":full_schema",
        ":updates",
    ],
    visibility = ["//services/db:__subpackages__"],
    deps = [
        "//environment:runfiles",
        "//strings:parse",
        "//strings:trim",
    ],
)

cc_test(
    name = "schema_test",
    srcs = ["schema_test.cc"],
    deps = [
        ":schema",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest_main",
        "@sqlite3",
    ],
)
