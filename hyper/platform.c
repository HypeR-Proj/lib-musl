/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <hyper/syscall.h>
#include <hyper/thread.h>
#include <hyper/std.h>
#include <hyper/stream.h>

/* Native owns the errno cell with the thread record, so access cannot fail
 * due to an allocation and all language runtimes observe the same cell. */
int *__errno_location(void)
{
	return hyper_runtime_errno_location();
}

weak_alias(__errno_location, ___errno_location);

_Noreturn void abort(void)
{
	hyper_process_exit(HYPER_NATIVE_STATUS_INTERNAL);
}

_Noreturn void _Exit(int status)
{
	hyper_process_exit(status);
}

static void diagnostic(const char *text)
{
	size_t written;
	hyper_runtime_stdio_write(2, text, strlen(text), &written);
}

_Noreturn void __assert_fail(const char *expr, const char *file, int line, const char *function)
{
	char number[3 * sizeof(unsigned) + 1];
	char *end = number + sizeof(number);
	*--end = 0;
	unsigned value = line < 0 ? 0u - (unsigned)line : (unsigned)line;
	do { *--end = '0' + value % 10; value /= 10; } while (value);
	diagnostic("Assertion failed: ");
	diagnostic(expr);
	diagnostic(" (");
	diagnostic(file);
	diagnostic(":");
	if (line < 0) diagnostic("-");
	diagnostic(end);
	diagnostic(", ");
	diagnostic(function);
	diagnostic(")\n");
	hyper_runtime_stream_fence(2);
	abort();
}

int posix_memalign(void **output, size_t alignment, size_t size)
{
	if (!output || alignment < sizeof(void *) || (alignment & (alignment - 1)))
		return EINVAL;
	if (size > SIZE_MAX - (alignment - 1))
		return ENOMEM;
	void *pointer = aligned_alloc(alignment, (size + alignment - 1) & ~(alignment - 1));
	if (!pointer)
		return ENOMEM;
	*output = pointer;
	return 0;
}

int clock_gettime(clockid_t id, struct timespec *out)
{
	if (!out || (id != CLOCK_REALTIME && id != CLOCK_MONOTONIC)) {
		errno = EINVAL;
		return -1;
	}
	hyper_call_result_t value = id == CLOCK_REALTIME ?
		hyper_clock_get_realtime() : hyper_clock_get_monotonic();
	if (value.status) {
		errno = value.status == HYPER_NATIVE_STATUS_NOT_SUPPORTED ? ENOTSUP :
			value.status == HYPER_NATIVE_STATUS_ACCESS_DENIED ? EPERM : EIO;
		return -1;
	}
	uint64_t seconds = id == CLOCK_REALTIME ? value.value0 : value.value0 / 1000000000;
	uint64_t nanos = id == CLOCK_REALTIME ? value.value1 : value.value0 % 1000000000;
	if (seconds > INT64_MAX || nanos >= 1000000000) {
		errno = EOVERFLOW;
		return -1;
	}
	*out = (struct timespec){.tv_sec = seconds, .tv_nsec = nanos};
	return 0;
}

int timespec_get(struct timespec *out, int base)
{
	return base == TIME_UTC && clock_gettime(CLOCK_REALTIME, out) == 0 ? base : 0;
}

time_t time(time_t *out)
{
	struct timespec value;
	time_t seconds = clock_gettime(CLOCK_REALTIME, &value) == 0 ? value.tv_sec : (time_t)-1;
	if (out)
		*out = seconds;
	return seconds;
}
