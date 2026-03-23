load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "register_service",
    hdrs = ["register_service.h"],
    visibility = ["//services:__subpackages__"],
    deps = [
        ":registry",
        ":registry_internal",
        "//services:service_base",
    ],
)

cc_library(
    name = "registry_internal",
    srcs = ["registry_internal.cc"],
    deps = ["//services:service_base"],
)

cc_library(
    name = "registry",
    hdrs = ["registry.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":registry_internal",
        "//services:service_base",
    ],
)

cc_test(
    name = "registry_test",
    srcs = ["registry_test.cc"],
    deps = [
        ":register_service",
        ":registry",
        "//services:service_base",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_utils",
    testonly = True,
    hdrs = ["test_utils.h"],
    visibility = ["//visibility:public"],
    deps = [":registry_internal"],
)
