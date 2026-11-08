/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hyper/startup.h>
#include <hyper/syscall.h>
#include "status.h"

static int path_size(const char *path, size_t *size)
{
	if (!path) { errno = EFAULT; return -1; }
	*size = strnlen(path, HYPER_NATIVE_DIRECTORY_MAX_PATH_BYTES + 1);
	if (!*size) { errno = ENOENT; return -1; }
	if (*size > HYPER_NATIVE_DIRECTORY_MAX_PATH_BYTES) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

static int acquire(const char *path, size_t *length, hyper_native_handle_t *directory)
{
	return path_size(path, length) ||
	       native_result(hyper_runtime_directory_acquire(path, *length, directory));
}

int chdir(const char *path)
{
	size_t length;
	if (path_size(path, &length)) return -1;
	return native_result(hyper_runtime_directory_change(path, length));
}

static char *canonical(const char *path, char *output, size_t size, int small_error)
{
	if (!size) { errno = EINVAL; return NULL; }
	size_t length;
	hyper_native_handle_t directory;
	if (acquire(path, &length, &directory)) return NULL;
	int allocated = output == NULL;
	if (size > PATH_MAX) size = PATH_MAX;
	if (allocated) {
		output = malloc(size);
		if (!output) {
			(void)hyper_handle_close(directory);
			errno = ENOMEM;
			return NULL;
		}
	}
	hyper_call_result_t result = hyper_directory_canonicalize(directory, path, length,
								 output, size - 1);
	(void)hyper_handle_close(directory);
	int error = result.status == HYPER_NATIVE_STATUS_BUFFER_TOO_SMALL ? small_error :
		native_errno(result.status);
	if (!error && (!result.value0 || result.value0 >= size || result.value1 ||
		      output[0] != '/' || memchr(output, 0, result.value0))) error = EIO;
	if (error) {
		if (allocated) free(output);
		errno = error;
		return NULL;
	}
	output[result.value0] = 0;
	return output;
}

char *getcwd(char *output, size_t size)
{
	return canonical(".", output, !output && !size ? PATH_MAX : size, ERANGE);
}

char *realpath(const char *restrict path, char *restrict output)
{
	return canonical(path, output, PATH_MAX, ENAMETOOLONG);
}

static int remove_path(const char *path, uint32_t directory_only)
{
	size_t length;
	hyper_native_handle_t directory;
	if (acquire(path, &length, &directory)) return -1;
	hyper_call_result_t result = hyper_directory_remove(directory, path, length, directory_only);
	(void)hyper_handle_close(directory);
	return native_result(result.status);
}

int unlink(const char *path) { return remove_path(path, 0); }
int rmdir(const char *path) { return remove_path(path, 1); }

int remove(const char *path)
{
	if (!unlink(path)) return 0;
	return errno == EISDIR ? rmdir(path) : -1;
}

static int move_path(const char *from, const char *to, int hardlink)
{
	size_t from_length, to_length;
	if (path_size(from, &from_length) || path_size(to, &to_length)) return -1;
	/* Relative names share one cwd snapshot. Absolute names use the delegated
	 * root, even if cwd carries narrower rights. A mixed operation snapshots
	 * cwd only once and acquires the immutable root separately. */
	hyper_native_handle_t directory, destination;
	if (native_result(hyper_runtime_directory_acquire(from, from_length, &directory))) return -1;
	destination = directory;
	int separate = (from[0] == '/') != (to[0] == '/');
	if (separate && native_result(hyper_runtime_directory_acquire(to, to_length, &destination))) {
		(void)hyper_handle_close(directory);
		return -1;
	}
	hyper_call_result_t result = hardlink ?
		hyper_directory_link(directory, from, from_length, destination, to, to_length) :
		hyper_directory_rename(directory, from, from_length, destination, to, to_length);
	if (separate) (void)hyper_handle_close(destination);
	(void)hyper_handle_close(directory);
	return native_result(result.status);
}

int rename(const char *from, const char *to) { return move_path(from, to, 0); }
int link(const char *from, const char *to) { return move_path(from, to, 1); }

int symlink(const char *target, const char *path)
{
	size_t target_length, length;
	if (path_size(target, &target_length)) return -1;
	hyper_native_handle_t directory;
	if (acquire(path, &length, &directory)) return -1;
	hyper_call_result_t result = hyper_directory_symlink(directory, path, length,
							    target, target_length);
	(void)hyper_handle_close(directory);
	return native_result(result.status);
}

ssize_t readlink(const char *restrict path, char *restrict output, size_t size)
{
	if (!output) { errno = EFAULT; return -1; }
	if (!size) { errno = EINVAL; return -1; }
	size_t length;
	hyper_native_handle_t directory;
	if (acquire(path, &length, &directory)) return -1;
	/* Native requires the entire target to fit. POSIX readlink instead copies
	 * a prefix, without a NUL terminator. Keep the caller's buffer unchanged
	 * until the one bounded Native read has completed. */
	char target[HYPER_NATIVE_DIRECTORY_MAX_PATH_BYTES];
	hyper_call_result_t result = hyper_directory_read_link(directory, path, length,
							      target, sizeof(target));
	(void)hyper_handle_close(directory);
	if (native_result(result.status)) return -1;
	if (result.value0 > sizeof(target) || result.value1) { errno = EIO; return -1; }
	size_t copied = result.value0 < size ? result.value0 : size;
	memcpy(output, target, copied);
	return (ssize_t)copied;
}

int truncate(const char *path, off_t size)
{
	if (size < 0) { errno = EINVAL; return -1; }
	size_t length;
	hyper_native_handle_t directory;
	if (acquire(path, &length, &directory)) return -1;
	hyper_call_result_t opened = hyper_directory_open_file(directory, path, length,
							   HYPER_NATIVE_RIGHT_WRITE);
	(void)hyper_handle_close(directory);
	if (native_result(opened.status)) return -1;
	hyper_call_result_t result = hyper_file_resize(opened.value0, (uint64_t)size);
	(void)hyper_handle_close(opened.value0);
	return native_result(result.status);
}
