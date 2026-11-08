#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 roolrz
# SPDX-License-Identifier: Apache-2.0
"""Exercise Native adapter boundaries on the host with deterministic Native calls."""
import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def check(workspace, output):
    output.mkdir(parents=True, exist_ok=True)
    headers = output / 'include'
    shutil.copytree(ROOT / 'include', headers, dirs_exist_ok=True)
    arch = {'arm64': 'aarch64', 'AMD64': 'x86_64'}.get(platform.machine(), platform.machine())
    shutil.copytree(ROOT / 'arch/generic/bits', headers / 'bits', dirs_exist_ok=True)
    shutil.copytree(ROOT / f'arch/{arch}/bits', headers / 'bits', dirs_exist_ok=True)
    with (headers / 'bits/alltypes.h').open('w') as target:
        subprocess.run(['sed', '-f', str(ROOT / 'tools/mkalltypes.sed'),
                        str(ROOT / f'arch/{arch}/bits/alltypes.h.in'),
                        str(ROOT / 'include/alltypes.h.in')], stdout=target, check=True)
    # Never interpose test adapters on the host C runtime's own startup/cleanup.
    names = ('getenv getpagesize sysconf sched_yield gettimeofday clock_gettime '
             'clock_nanosleep nanosleep sleep usleep thrd_sleep thrd_yield '
             'chdir getcwd realpath unlink rmdir rename link symlink readlink truncate').split()
    command = [os.environ.get('HOST_CC', 'cc'),
               '-std=c17', '-D_GNU_SOURCE=1', '-O1', '-g', '-fno-builtin',
               '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
               '-fno-sanitize-recover=all']
    command += [f'-D{name}=hyper_test_{name}' for name in names]
    for directory in (headers, workspace / 'stdlib/system/include', workspace / 'sdk/abi/include'):
        command += ['-I', str(directory)]
    command += [str(ROOT / 'hyper' / name) for name in
                ('environment.c', 'system.c', 'time.c', 'path.c', 'adapter-test.c')]
    binary = output / 'adapter-test'
    subprocess.run(command + ['-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    print('musl Native adapter error, lifetime and boundary tests passed', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    check(args.workspace.resolve(), args.output.resolve())
