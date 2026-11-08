/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits.h>
#include <sched.h>
#include <unistd.h>
#include <hyper/system.h>
#include <hyper/syscall.h>
#include "status.h"

int getpagesize(void)
{
	size_t size;
	if (native_result(hyper_page_size(&size))) return -1;
	if (size > INT_MAX) { errno = EOVERFLOW; return -1; }
	return (int)size;
}

long sysconf(int name)
{
	if (name == _SC_PAGESIZE) return getpagesize();
	errno = EINVAL;
	return -1;
}

int sched_yield(void)
{
	return native_result(hyper_thread_yield());
}
