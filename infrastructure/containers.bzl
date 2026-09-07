"""Container packaging macros for howling-trader services."""

load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load", "oci_push")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

def package_binary(
        name,
        binary = None,
        base = "@ubuntu_resolute",
        package_dir = None,
        entrypoint = None,
        env = None,
        tars = None,
        repo_tags = None,
        remote_tags = None,
        image_name = None,
        include_runfiles = True,
        visibility = None):
    """Packages a binary into an OCI container image with load and push targets.

    Args:
        name: Target prefix for the generated rules.
        binary: The binary target to package. Defaults to ":" + name if not
            specified.
        base: The base image label. Defaults to "@ubuntu_resolute".
        package_dir: Directory inside the container where the binary will be
            installed. Defaults to "/" + native.package_name().
        entrypoint: Command list for container entrypoint. Defaults to
            [package_dir + "/" + binary_name].
        env: Dictionary of environment variables. RUNFILES_DIR is set by
            default.
        tars: Additional tar layers to include in the image.
        repo_tags: List of repository tags for local loading (oci_load).
        remote_tags: List of tags for remote pushing (oci_push). Defaults to
            ["latest"].
        image_name: Base image name used for default repo_tags.
        include_runfiles: Whether to include runfiles in the tar layer.
            Defaults to True.
        visibility: Target visibility list.
    """

    # Resolve binary target and binary name.
    actual_binary = binary if binary else ":" + name
    if actual_binary.startswith(":"):
        binary_name = actual_binary[1:]
    elif ":" in actual_binary:
        binary_name = actual_binary.split(":")[-1]
    elif actual_binary.startswith("//"):
        binary_name = actual_binary.split("/")[-1]
    else:
        binary_name = actual_binary
        actual_binary = ":" + actual_binary

    # Resolve container package directory.
    pkg_path = native.package_name()
    resolved_package_dir = package_dir if package_dir else (
        "/" + pkg_path if pkg_path else "/app"
    )

    # Create tar layer with binary and runfiles.
    layer_name = name + "_layer"
    pkg_tar(
        name = layer_name,
        srcs = [actual_binary],
        include_runfiles = include_runfiles,
        package_dir = resolved_package_dir,
        visibility = visibility,
    )

    # Resolve entrypoint.
    resolved_entrypoint = entrypoint if entrypoint else [
        resolved_package_dir + "/" + binary_name,
    ]

    # Resolve environment variables.
    image_env = {
        "RUNFILES_DIR": (
            resolved_package_dir + "/" + binary_name + ".runfiles"
        ),
    }
    if env:
        image_env.update(env)

    # Combine tars.
    image_tars = [":" + layer_name]
    if tars:
        image_tars.extend(tars)

    # Create OCI image.
    image_target_name = name + "_image"
    oci_image(
        name = image_target_name,
        base = base,
        entrypoint = resolved_entrypoint,
        env = image_env,
        tars = image_tars,
        visibility = visibility,
    )

    # If the rule name doesn't conflict with the binary name, alias it to image.
    if name != binary_name:
        native.alias(
            name = name,
            actual = ":" + image_target_name,
            visibility = visibility,
        )

    # Resolve repo tags for local loading.
    resolved_image_name = image_name if image_name else "howling-trader"
    resolved_repo_tags = repo_tags if repo_tags else [
        resolved_image_name + ":latest",
    ]

    # Create oci_load target.
    oci_load(
        name = name + "_load",
        image = ":" + image_target_name,
        repo_tags = resolved_repo_tags,
        visibility = visibility,
    )

    # Create oci_push target.
    resolved_remote_tags = remote_tags if remote_tags else ["latest"]
    oci_push(
        name = name + "_push",
        image = ":" + image_target_name,
        remote_tags = resolved_remote_tags,
        visibility = visibility,
    )
