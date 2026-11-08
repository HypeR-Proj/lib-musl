/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HYPER_MUSL_STATUS_H
#define HYPER_MUSL_STATUS_H
#include <errno.h>
#include <hyper/native.h>

static inline int native_errno(hyper_native_status_t status)
{
	switch (status) {
	case HYPER_NATIVE_STATUS_OK: return 0;
	case HYPER_NATIVE_STATUS_INVALID_ARGUMENT: return EINVAL;
	case HYPER_NATIVE_STATUS_BAD_HANDLE: return EBADF;
	case HYPER_NATIVE_STATUS_ACCESS_DENIED: return EACCES;
	case HYPER_NATIVE_STATUS_NOT_SUPPORTED: return ENOTSUP;
	case HYPER_NATIVE_STATUS_NO_MEMORY: return ENOMEM;
	case HYPER_NATIVE_STATUS_FAULT: return EFAULT;
	case HYPER_NATIVE_STATUS_RESOURCE_LIMIT: return EAGAIN;
	case HYPER_NATIVE_STATUS_BUSY: return EBUSY;
	case HYPER_NATIVE_STATUS_TIMED_OUT: return ETIMEDOUT;
	case HYPER_NATIVE_STATUS_CANCELLED: return EINTR;
	case HYPER_NATIVE_STATUS_WOULD_BLOCK: return EAGAIN;
	case HYPER_NATIVE_STATUS_BUFFER_TOO_SMALL: return ERANGE;
	case HYPER_NATIVE_STATUS_PEER_CLOSED: return EPIPE;
	case HYPER_NATIVE_STATUS_NOT_FOUND: return ENOENT;
	case HYPER_NATIVE_STATUS_ALREADY_EXISTS: return EEXIST;
	case HYPER_NATIVE_STATUS_NOT_EMPTY: return ENOTEMPTY;
	case HYPER_NATIVE_STATUS_NOT_DIRECTORY: return ENOTDIR;
	case HYPER_NATIVE_STATUS_IS_DIRECTORY: return EISDIR;
	case HYPER_NATIVE_STATUS_SYMLINK_LOOP: return ELOOP;
	case HYPER_NATIVE_STATUS_CROSS_DEVICE: return EXDEV;
	case HYPER_NATIVE_STATUS_NO_SPACE: return ENOSPC;
	case HYPER_NATIVE_STATUS_READ_ONLY: return EROFS;
	default: return EIO;
	}
}

static inline int native_result(hyper_native_status_t status)
{
	if (!status) return 0;
	errno = native_errno(status);
	return -1;
}
#endif
