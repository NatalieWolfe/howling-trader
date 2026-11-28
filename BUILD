load("@rules_shell//shell:sh_test.bzl", "sh_test")

exports_files(["schedule_trade.sh"])

sh_test(
    name = "schedule_trade_test",
    srcs = ["schedule_trade_test.sh"],
    data = ["schedule_trade.sh"],
)
