/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: MIT
 */
#include <sys/random.h>
#include <unistd.h>
#include <limits.h>
#include <hyper/random.h>
#include "status.h"

static int random_errno(int64_t status)
{
	/* A failed entropy provider is an I/O failure, not a pipe API. */
	return status == HYPER_NATIVE_STATUS_PEER_CLOSED || status == HYPER_NATIVE_STATUS_INTERNAL
		       ? EIO
		       : native_errno(status);
}

ssize_t getrandom(void *buffer, size_t length, unsigned flags)
{
	if ((flags & ~GRND_NONBLOCK) || length > SSIZE_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (length && !buffer) {
		errno = EFAULT;
		return -1;
	}
	hyper_call_result_t result =
		hyper_random_read(buffer, length, flags & GRND_NONBLOCK ? HYPER_RANDOM_NONBLOCK : 0,
				  HYPER_NATIVE_DEADLINE_INFINITE);
	if (result.status) {
		errno = random_errno(result.status);
		return -1;
	}
	if (result.value0 > length || result.value1 || (length && !result.value0)) {
		errno = EIO;
		return -1;
	}
	return (ssize_t)result.value0;
}

int getentropy(void *buffer, size_t length)
{
	if (length > 256) {
		errno = EIO;
		return -1;
	}
	if (length && !buffer) {
		errno = EFAULT;
		return -1;
	}
	int64_t status = hyper_random_fill(buffer, length, HYPER_NATIVE_DEADLINE_INFINITE);
	if (status) {
		errno = random_errno(status);
		return -1;
	}
	return 0;
}
