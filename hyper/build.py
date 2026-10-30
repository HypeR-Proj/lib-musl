#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 roolrz
# SPDX-License-Identifier: Apache-2.0
"""Build the explicitly supported musl subset, without Linux ABI objects."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    [
        "src/ctype/" + name + ".c"
        for name in "isalnum isalpha isascii isblank iscntrl isdigit isgraph islower isprint "
        "ispunct isspace isupper isxdigit toascii tolower toupper".split()
    ]
    + [
        "src/string/" + name + ".c"
        for name in "stpcpy stpncpy strcpy strncpy strcat strncat strrchr strspn strcspn strpbrk "
        "strstr strchrnul strtok strtok_r strnlen strdup strndup memrchr strerror_r".split()
    ]
    + [
        "src/stdlib/" + name + ".c"
        for name in "abs labs llabs div ldiv lldiv atoi atol atoll bsearch qsort qsort_nr strtol".split()
    ]
    + ["src/internal/intscan.c", "hyper/platform.c", "hyper/strerror.c"]
)


def build(sdk, output, install=False, jobs=4):
    sdk, output = sdk.resolve(), output.resolve()
    identity = dict(
        line.split("=", 1)
        for line in (sdk / "share/hyper/manifest").read_text().splitlines()
        if "=" in line
    )
    arch = identity["architecture"]
    if arch not in ("aarch64", "riscv64"):
        raise ValueError("musl port supports AArch64 and RISC-V Native only")
    output.mkdir(parents=True, exist_ok=True)
    headers = output / "include"
    shutil.copytree(ROOT / "include", headers, dirs_exist_ok=True)
    (headers / "alltypes.h.in").unlink(missing_ok=True)
    shutil.copytree(ROOT / "arch/generic/bits", headers / "bits", dirs_exist_ok=True)
    shutil.copytree(ROOT / f"arch/{arch}/bits", headers / "bits", dirs_exist_ok=True)
    with (headers / "bits/alltypes.h").open("w") as generated:
        subprocess.run(
            [
                "sed",
                "-f",
                str(ROOT / "tools/mkalltypes.sed"),
                str(ROOT / f"arch/{arch}/bits/alltypes.h.in"),
                str(ROOT / "include/alltypes.h.in"),
            ],
            stdout=generated,
            check=True,
        )
    for path in headers.rglob("*.in"):
        path.unlink()
    # Never publish Linux syscall numbers as a Native syscall interface.
    (headers / "bits/syscall.h").unlink(missing_ok=True)
    compiler = os.environ.get("HYPER_CLANG", "clang")
    flags = [
        compiler,
        f"--target={arch}-none-elf",
        "-std=c17",
        "-O2",
        "-g",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-stack-protector",
        "-fPIC",
        "-ffunction-sections",
        "-fdata-sections",
        "-D_XOPEN_SOURCE=700",
        "-DHYPER_MUSL_STRING_ONLY=1",
    ]
    if arch == "riscv64":
        flags += ["-march=rv64gc", "-mabi=lp64d", "-mno-relax"]
    includes = [
        ROOT / "hyper/include",
        ROOT / f"arch/{arch}",
        ROOT / "arch/generic",
        ROOT / "src/include",
        ROOT / "src/internal",
        headers,
        sdk / "include",
    ]
    flags += [arg for path in includes for arg in ("-I", str(path))]
    fingerprint = hashlib.sha256(json.dumps(flags).encode())
    fingerprint.update(subprocess.check_output([compiler, "--version"]))
    inputs = {ROOT / source for source in SOURCES}
    inputs.add(Path(__file__))
    for directory in includes:
        inputs.update(path for path in directory.rglob("*") if path.is_file())
    for path in sorted(inputs):
        fingerprint.update(str(path).encode())
        fingerprint.update(path.read_bytes())
    stamp, archive = output / "build.sha256", output / "libc.a"
    if (
        not archive.exists()
        or not stamp.exists()
        or stamp.read_text() != fingerprint.hexdigest()
    ):

        def compile_source(source):
            obj = output / (source.replace("/", "_") + ".o")
            subprocess.run(
                flags + ["-c", str(ROOT / source), "-o", str(obj)], check=True
            )
            return str(obj)

        with ThreadPoolExecutor(max_workers=jobs) as pool:
            objects = list(pool.map(compile_source, SOURCES))
        temporary = output / "libc.tmp.a"
        temporary.unlink(missing_ok=True)
        archiver = os.environ.get(
            "LLVM_AR",
            str(Path(shutil.which(compiler) or compiler).with_name("llvm-ar")),
        )
        subprocess.run([archiver, "rcs", str(temporary)] + objects, check=True)
        temporary.replace(archive)
        stamp.write_text(fingerprint.hexdigest())
    if install:
        shutil.copytree(headers, sdk / "include", dirs_exist_ok=True)
        shutil.copyfile(archive, sdk / "lib/libc.a")
        licenses = sdk / "share/licenses/musl"
        licenses.mkdir(parents=True, exist_ok=True)
        for name in ("COPYRIGHT", "LICENSE-HypeR", "UPSTREAM.json"):
            shutil.copyfile(ROOT / name, licenses / name)
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--install", action="store_true")
    args = parser.parse_args()
    build(args.sdk, args.output, args.install)
