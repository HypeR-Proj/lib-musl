<!-- SPDX-FileCopyrightText: 2026 roolrz -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# musl for HypeR Native

This is a source fork of musl 1.2.5, with a deliberately limited Native port.
The imported tree and its tests remain available; `hyper/build.py` explicitly
selects the implemented source files. Builds do not fetch upstream archives.
The [upstream identity](UPSTREAM.json) records the verified source archive.
musl retains its [MIT license and notices](COPYRIGHT); new Native adapters use
Apache-2.0. No Linux syscall, pthread, allocator or loader implementation is built.

The SDK installs `libc.a` and C headers. HypeR's existing `libsystem` owns startup,
the heap, basic memory/string functions and Native threads. This port reuses
that ownership and adds musl's character classification, string operations,
integer conversions, sorting/searching, C-locale error messages, per-thread
`errno`, `abort`, `_Exit`, aligned allocation adaptation, `time`, `timespec_get`
and realtime/monotonic `clock_gettime`.

The Native adapters also provide:

- `getenv` over the same process-lifetime startup environment read by Rust;
- `gettimeofday`, `nanosleep`, `sleep`, `usleep`, `clock_nanosleep`, and the
  standalone C11 `thrd_sleep` / `thrd_yield` operations;
- `sched_yield`, `getpagesize`, and `sysconf(_SC_PAGESIZE)`;
- `chdir`, `getcwd`, `realpath`, `unlink`, `rmdir`, `rename`, `link`, `symlink`,
  `readlink`, and pathname-based `truncate`; and
- UTC `gmtime` / `gmtime_r`, `timegm`, and `difftime` without a timezone loader.

Sleep uses the Native monotonic clock. Relative `CLOCK_REALTIME` sleeps have
elapsed-time semantics; absolute sleeps support `CLOCK_MONOTONIC` only.
`clock_nanosleep` returns an error number without changing errno, reports
`ENOTSUP` for absolute realtime waits, and rejects unrepresentable finite
deadlines with `EOVERFLOW`. Native cancellation maps to interruption; no POSIX
signal delivery or restart behavior is implied. Unimplemented `sysconf` keys
return `-1` with `EINVAL`, rather than guessed configuration values.

Path operations share libsystem's cwd and delegated root, including its UTF-8
path rules and capability checks. A two-path operation uses one cwd snapshot.
`getcwd(NULL, 0)` and `realpath(path, NULL)` allocate a result that the caller
frees. `readlink` returns a possibly truncated prefix without a terminator.
Backend errors and unavailable operations propagate through errno. No fd table,
filesystem permission emulation, or additional ambient authority is introduced.
`getenv` does not publish mutable `environ` or implement environment mutation.

The integer scanner is adapted to read strings directly, retaining musl's
conversion and overflow logic without importing file-descriptor stdio.
The supported implementation list is [SOURCES](hyper/build.py); the basic
memory and allocation functions come from the
[Native runtime](https://github.com/HypeR-Proj/lib-system).

This is not a complete hosted C/POSIX implementation. File-descriptor stdio,
formatted input/output, floating-point string conversion, the math library,
locale selection, wide-character conversion, C11 thread creation/synchronization,
pthreads, signals, environment mutation and `exit`/`atexit` are not implemented. Retained upstream
headers may declare these APIs; using them fails to link. There are no success
stubs. Allocation retains Native's contract: a failure returns NULL without
setting errno. Normal `main` return and `_Exit` are available; no exit callbacks
or stream flushing are implied.

## Build and use

In a repo-managed HypeR workspace:

```sh
make sdk ARCH=aarch64
target/sdk/aarch64/bin/hyper-cc -std=c17 program.c -o program
python3 project.py test lib-musl
```

`main` is the standard C entry point. `hyper-clang` remains the low-level Native
entry point for existing Native and Rust consumers. The supported targets are
AArch64 and RISC-V. Standard libraries are static archives; the executable may
use the shared Native runtime or `HYPER_LINK_MODE=static`. Standard C/C++ shared
objects are outside this initial port.

The module tests exercise adapter errors and boundary cases on the host with
UBSan, run musl algorithms on Linux hosts, build both Native targets,
and verify supported links and rejected unsupported calls. System tests run
the same C probe inside HypeR. F2FS remains a separate consumer of this public SDK.
