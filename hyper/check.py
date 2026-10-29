#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 roolrz
# SPDX-License-Identifier: Apache-2.0
"""Exercise the C algorithms and the installed Native ABI on both targets."""
import argparse
import importlib.util
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent


def check(workspace):
    workspace = workspace.resolve()
    compiler = os.environ.get("HYPER_CLANG", os.environ.get("CLANG", "clang"))
    linker = subprocess.check_output(
        ["sh", str(workspace / "scripts/find-llvm-tool.sh"), "ld.lld"], text=True
    ).strip()
    spec = importlib.util.spec_from_file_location("musl_build", HERE / "build.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    for arch in ("aarch64", "riscv64"):
        subprocess.run(["make", "sdk", f"ARCH={arch}"], cwd=workspace, check=True)
        sdk, output = (
            workspace / f"target/sdk/{arch}",
            workspace / f"target/lib-musl-test/{arch}",
        )
        output.mkdir(parents=True, exist_ok=True)
        environment = {
            **os.environ,
            "HYPER_SYSROOT": str(sdk),
            "HYPER_LD": linker,
            "HYPER_CLANG": compiler,
        }
        for mode in ("static", "dynamic"):
            subprocess.run(
                [
                    str(sdk / "bin/hyper-cc"),
                    "-D_GNU_SOURCE=1",
                    "-std=c17",
                    "-O2",
                    str(HERE / "test.c"),
                    "-o",
                    str(output / mode),
                ],
                env={**environment, "HYPER_LINK_MODE": mode},
                check=True,
            )
        rejected = output / "unsupported.c"
        rejected.write_text(
            '#include <stdio.h>\nint main(void) { return fopen("/file", "r") == 0; }\n'
        )
        result = subprocess.run(
            [
                str(sdk / "bin/hyper-cc"),
                str(rejected),
                "-o",
                str(output / "unsupported"),
            ],
            env=environment,
            capture_output=True,
            text=True,
        )
        if result.returncode == 0 or "fopen" not in result.stderr:
            raise RuntimeError("unsupported stdio must fail to link")
        print(
            f"musl {arch}: supported links and unsupported API rejection passed",
            flush=True,
        )
    # Host checks execute the imported algorithms, not host-libc replacements.
    # Native heap/TLS/clock behavior is covered separately in the booted probe.
    if sys.platform != "linux":
        print(
            "host algorithm tests require Linux ELF aliases; Native target checks passed",
            flush=True,
        )
        return
    host = workspace / "target/lib-musl-test/host"
    host.mkdir(parents=True, exist_ok=True)
    native_arch = {"arm64": "aarch64", "AMD64": "x86_64"}.get(
        platform.machine(), platform.machine()
    )
    headers = host / "include"
    shutil.copytree(build.ROOT / "include", headers, dirs_exist_ok=True)
    shutil.copytree(
        build.ROOT / "arch/generic/bits", headers / "bits", dirs_exist_ok=True
    )
    shutil.copytree(
        build.ROOT / f"arch/{native_arch}/bits", headers / "bits", dirs_exist_ok=True
    )
    with (headers / "bits/alltypes.h").open("w") as generated:
        subprocess.run(
            [
                "sed",
                "-f",
                str(build.ROOT / "tools/mkalltypes.sed"),
                str(build.ROOT / f"arch/{native_arch}/bits/alltypes.h.in"),
                str(build.ROOT / "include/alltypes.h.in"),
            ],
            stdout=generated,
            check=True,
        )
    errno = host / "errno.c"
    errno.write_text(
        "static _Thread_local int value; int *__errno_location(void) { return &value; } "
        "int *___errno_location(void) { return &value; }\n"
    )
    includes = [
        HERE / "include",
        build.ROOT / f"arch/{native_arch}",
        build.ROOT / "arch/generic",
        build.ROOT / "src/include",
        build.ROOT / "src/internal",
        headers,
    ]
    flags = [
        compiler,
        "-std=c17",
        "-D_XOPEN_SOURCE=700",
        "-DHYPER_MUSL_STRING_ONLY=1",
        "-fno-builtin",
        "-O2",
    ]
    flags += [arg for directory in includes for arg in ("-I", str(directory))]
    files = [
        str(build.ROOT / source)
        for source in build.SOURCES
        if source != "hyper/platform.c"
    ]
    subprocess.run(
        flags + files + [str(errno), str(HERE / "test.c"), "-o", str(host / "test")],
        check=True,
    )
    subprocess.run([str(host / "test")], check=True)
    print(
        "musl host conversion, overflow, sorting, string and C-locale tests passed",
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    check(parser.parse_args().workspace)
