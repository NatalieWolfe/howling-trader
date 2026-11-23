load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("//infrastructure:multirun.bzl", "multirun")

package_group(
    name = "visibility",
    packages = ["//infrastructure/..."],
)

exports_files([
    "containers.bzl",
    "docker-compose.yml",
    "multirun.bzl",
    "nginx.conf",
])

sh_binary(
    name = "compose-up",
    srcs = ["local-up.sh"],
    data = [
        "docker-compose.yml",
        "nginx.conf",
        "//grafana:dashboards",
        "//grafana:provisioning",
        "//net:local.wolfe.dev.crt",
        "//net:local.wolfe.dev.key",
    ],
)

sh_binary(
    name = "compose-restart",
    srcs = ["local-restart.sh"],
    data = [
        "docker-compose.yml",
    ],
)

sh_binary(
    name = "local-down",
    srcs = ["local-down.sh"],
    data = [
        "docker-compose.yml",
    ],
)

_SERVICE_IMAGES = [
    "//services/db:schema_upgrade_image_load",
    "//services/oauth:auth_refresh_image_load",
    "//services/oauth:server_image_load",
    "//services/simulation:server_image_load",
]

multirun(
    name = "load-service-images",
    # parallel = True,
    commands = _SERVICE_IMAGES,
)

multirun(
    name = "load-images",
    # parallel = True,
    commands = _SERVICE_IMAGES + [
        "//howling_tools:execute_image_load",
    ],
)

multirun(
    name = "services-up",
    args = [
        "-d",
        "gateway",
        "grafana",
        "simulation_service",
    ],
    commands = [
        ":load-service-images",
        ":compose-up",
    ],
)

multirun(
    name = "local-up",
    commands = [
        ":load-images",
        ":compose-up",
    ],
)

multirun(
    name = "local-restart",
    commands = [
        ":load-images",
        ":compose-restart",
    ],
)
