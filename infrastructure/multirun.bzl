"""Starlark rule to execute multiple Bazel targets sequentially or in parallel."""

def _multirun_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name + "_launcher.sh")

    runfiles = ctx.runfiles(files = [])
    bash_runfiles = ctx.attr._bash_runfiles[DefaultInfo].default_runfiles
    runfiles = runfiles.merge(bash_runfiles)

    script_lines = [
        "#!/bin/bash",
        "set -euo pipefail",
        "",
        "# --- begin runfiles.bash initialization v3 ---",
        "set +e",
        "f=bazel_tools/tools/bash/runfiles/runfiles.bash",
        "source \"${RUNFILES_DIR:-/dev/null}/$f\" 2>/dev/null || \\",
        "  source \"$(grep -sm1 \"^$f \" \"${RUNFILES_MANIFEST_FILE:-/dev/null}\" | cut -f2- -d' ')\" 2>/dev/null || \\",
        "  source \"$0.runfiles/$f\" 2>/dev/null || \\",
        "  source \"$(grep -sm1 \"^$f \" \"$0.runfiles_manifest\" | cut -f2- -d' ')\" 2>/dev/null || \\",
        "  source \"$(grep -sm1 \"^$f \" \"$0.exe.runfiles_manifest\" | cut -f2- -d' ')\" 2>/dev/null || \\",
        "  { echo >&2 \"ERROR: cannot find $f\"; exit 1; }; f=; set -e",
        "# --- end runfiles.bash initialization v3 ---",
        "",
    ]

    cmd_vars = []
    total_commands = len(ctx.attr.commands)
    for i, cmd in enumerate(ctx.attr.commands):
        exe = cmd[DefaultInfo].files_to_run.executable
        if not exe:
            fail("Target %s is not an executable binary." % cmd.label)

        runfiles = runfiles.merge(cmd[DefaultInfo].default_runfiles)
        runfiles = runfiles.merge(ctx.runfiles(files = [exe]))

        short_path = exe.short_path
        if short_path.startswith("../"):
            rloc_path = short_path[3:]
        else:
            rloc_path = "_main/" + short_path

        var_name = "CMD_%d" % i
        cmd_vars.append(var_name)
        script_lines.append('%s="$(rlocation "%s")"' % (var_name, rloc_path))

    script_lines.append("")

    if ctx.attr.parallel:
        script_lines.extend([
            "pids=()",
            "trap 'for pid in \"${pids[@]}\"; do kill \"$pid\" 2>/dev/null || true; done' INT TERM EXIT",
            "",
        ])
        for var_name in cmd_vars:
            script_lines.append('"$%s" "$@" &' % var_name)
            script_lines.append("pids+=($!)")

        script_lines.extend([
            "",
            "failed=0",
            'for pid in "${pids[@]}"; do',
            '    wait "$pid" || failed=1',
            "done",
            "",
            "trap - INT TERM EXIT",
            'if [ "$failed" -ne 0 ]; then',
            '    echo >&2 "multirun: One or more parallel commands failed."',
            "    exit 1",
            "fi",
        ])
    else:
        for i, var_name in enumerate(cmd_vars):
            if i == total_commands - 1:
                script_lines.append('exec "$%s" "$@"' % var_name)
            else:
                script_lines.append('"$%s"' % var_name)
            script_lines.append("")

    ctx.actions.write(
        output = out,
        content = "\n".join(script_lines),
        is_executable = True,
    )

    return [
        DefaultInfo(
            executable = out,
            runfiles = runfiles,
        ),
    ]

multirun = rule(
    implementation = _multirun_impl,
    executable = True,
    attrs = {
        "commands": attr.label_list(
            mandatory = True,
            allow_empty = False,
            doc = "List of executable targets to run.",
            cfg = "target",
        ),
        "parallel": attr.bool(
            default = False,
            doc = "If True, executes commands concurrently in background processes and waits for all to finish.",
        ),
        "_bash_runfiles": attr.label(
            default = "@bazel_tools//tools/bash/runfiles",
        ),
    },
    doc = "Runs multiple executable targets in sequence or in parallel with 'bazel run'.",
)
