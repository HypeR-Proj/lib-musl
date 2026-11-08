/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <threads.h>
#include <unistd.h>
#include <hyper/syscall.h>
#include "status.h"

int gettimeofday(struct timeval *restrict out, void *restrict timezone)
{
	(void)timezone; /* Obsolete timezone argument, as in upstream musl. */
	if (!out) return 0;
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now)) return -1;
	*out = (struct timeval){now.tv_sec, now.tv_nsec / 1000};
	return 0;
}

/* Native deadlines are monotonic; UINT64_MAX is the infinite-wait sentinel.
 * Do not approximate absolute realtime sleeps with a sampled clock offset. */
int clock_nanosleep(clockid_t clock, int flags, const struct timespec *request,
		    struct timespec *remaining)
{
	if ((clock != CLOCK_MONOTONIC && clock != CLOCK_REALTIME) ||
	    (flags != 0 && flags != TIMER_ABSTIME)) return EINVAL;
	if (!request) return EFAULT;
	if (request->tv_sec < 0 || request->tv_nsec < 0 || request->tv_nsec >= 1000000000)
		return EINVAL;
	if (clock == CLOCK_REALTIME && flags == TIMER_ABSTIME) return ENOTSUP;
	uint64_t seconds = request->tv_sec, nanos = request->tv_nsec;
	if (seconds > (UINT64_MAX - 1 - nanos) / 1000000000) return EOVERFLOW;
	uint64_t duration = seconds * 1000000000 + nanos;
	hyper_call_result_t now = hyper_clock_get_monotonic();
	if (now.status) return native_errno(now.status);
	uint64_t deadline = duration;
	if (!flags) {
		if (duration >= UINT64_MAX - now.value0) return EOVERFLOW;
		deadline += now.value0;
	}
	if (deadline <= now.value0) return 0;
	int error = native_errno(hyper_thread_sleep(deadline));
	if (error == EINTR && !flags && remaining) {
		now = hyper_clock_get_monotonic();
		/* No fabricated remaining time if the clock itself failed. */
		if (now.status) return native_errno(now.status);
		uint64_t left = deadline > now.value0 ? deadline - now.value0 : 0;
		*remaining = (struct timespec){left / 1000000000, left % 1000000000};
	}
	return error; /* clock_nanosleep returns an error number, not errno. */
}

int nanosleep(const struct timespec *request, struct timespec *remaining)
{
	int error = clock_nanosleep(CLOCK_MONOTONIC, 0, request, remaining);
	if (!error) return 0;
	errno = error;
	return -1;
}

unsigned sleep(unsigned seconds)
{
	struct timespec request = {seconds, 0}, remaining = request;
	if (!nanosleep(&request, &remaining)) return 0;
	return (unsigned)remaining.tv_sec + (remaining.tv_nsec != 0);
}

int usleep(unsigned microseconds)
{
	struct timespec request = {microseconds / 1000000, (microseconds % 1000000) * 1000};
	return nanosleep(&request, NULL);
}

int thrd_sleep(const struct timespec *request, struct timespec *remaining)
{
	int error = clock_nanosleep(CLOCK_MONOTONIC, 0, request, remaining);
	return !error ? 0 : error == EINTR ? -1 : -2;
}

void thrd_yield(void)
{
	(void)hyper_thread_yield();
}
