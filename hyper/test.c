/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __HYPER__
#include <sched.h>
#include <sys/time.h>
#include <threads.h>
#include <unistd.h>
#include <hyper/startup.h>
#include <hyper/system.h>
#include <hyper/syscall.h>
#include <hyper/stream.h>
#include <hyper/std.h>
#endif

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static int compare(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

#ifdef __HYPER__
static int paths(void)
{
	hyper_call_result_t identity = hyper_process_get_current_id();
	CHECK(!identity.status);
	char original[80] = "/tmp/musl-adapters-", other[80], symbolic[80];
	size_t prefix = strlen(original);
	for (unsigned i = 0; i < 16; ++i)
		original[prefix + i] = "0123456789abcdef"[(identity.value0 >> (60 - 4 * i)) & 15];
	original[prefix + 16] = 0;
	strcpy(other, original); strcat(other, "-other");
	strcpy(symbolic, original); strcat(symbolic, "-symlink");
	hyper_native_handle_t root;
	CHECK(!hyper_runtime_directory_acquire(original, strlen(original), &root));
	hyper_call_result_t opened = hyper_directory_create_file(root, original, strlen(original),
		HYPER_NATIVE_RIGHT_READ | HYPER_NATIVE_RIGHT_WRITE | HYPER_NATIVE_RIGHT_INSPECT, 0600);
	CHECK(!opened.status);
	CHECK(!hyper_file_write_at(opened.value0, 0, 0, "hello", 5).status);
	CHECK(!truncate(original, 2));
	char contents[5] = {0};
	hyper_call_result_t read = hyper_file_read_at(opened.value0, 0, contents, sizeof(contents));
	CHECK(!read.status && read.value0 == 2 && !memcmp(contents, "he", 2));
	CHECK(!link(original, other));
	CHECK(!unlink(original));
	CHECK(!rename(other, original));
	CHECK(!symlink(original, symbolic));
	memset(contents, '!', sizeof(contents));
	CHECK(readlink(symbolic, contents, 4) == 4 && !memcmp(contents, "/tmp!", 5));
	CHECK(!remove(symbolic) && !unlink(original));
	CHECK(!hyper_handle_close(opened.value0));
	CHECK(!hyper_directory_create_directory(root, other, strlen(other), 0700).status);
	char *old = getcwd(NULL, 0);
	CHECK(old && !chdir(other));
	char *current = getcwd(NULL, 0);
	CHECK(current && !strcmp(current, other));
	CHECK(!chdir(old));
	free(current); free(old);
	CHECK(!rmdir(other));
	CHECK(!hyper_directory_create_directory(root, other, strlen(other), 0700).status);
	CHECK(!remove(other));
	CHECK(!hyper_handle_close(root));
	return 0;
}
#endif

int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "assert"))
		assert(!"standard C assertion probe");
	char *end;
	errno = 0;
	CHECK(strtol("-9223372036854775808!", &end, 10) == LONG_MIN && *end == '!' && !errno);
	CHECK(strtoull("18446744073709551615!", &end, 10) == ULLONG_MAX && *end == '!' && !errno);
	CHECK(strtol("9223372036854775808", &end, 10) == LONG_MAX && errno == ERANGE);
	errno = 0;
	CHECK(strtol("-9223372036854775809", &end, 10) == LONG_MIN && errno == ERANGE);
	const char *bad = " +no-number";
	CHECK(strtol(bad, &end, 10) == 0 && end == bad);
	CHECK(strtol("0x", &end, 0) == 0 && *end == 'x');
	CHECK(strtol(" -0x2a trailing", &end, 0) == -42 && *end == ' ');
	CHECK(strtol("077", &end, 0) == 63 && !*end);
	CHECK(strtol("z", &end, 36) == 35 && !*end);
	CHECK(strtoul("-1", &end, 10) == ULONG_MAX && !*end);
	CHECK(strtol("42", &end, 1) == 0 && errno == EINVAL);
	int values[] = {9, -2, 0, 4, 4, INT_MIN, INT_MAX};
	qsort(values, sizeof(values) / sizeof(*values), sizeof(*values), compare);
	for (unsigned i = 1; i < sizeof(values) / sizeof(*values); ++i)
		CHECK(values[i - 1] <= values[i]);
	int key = 4;
	CHECK(bsearch(&key, values, 7, sizeof(*values), compare));
	char text[32];
	strcpy(text, "one");
	strcat(text, ":two:three");
	CHECK(strcmp(text, "one:two:three") == 0);
	CHECK(strstr(text, "two") == text + 4);
	CHECK(strrchr(text, ':') == text + 7 && strnlen(text, 3) == 3);
	char *save;
	CHECK(!strcmp(strtok_r(text, ":", &save), "one"));
	CHECK(!strcmp(strtok_r(NULL, ":", &save), "two"));
	CHECK(!strcmp(strtok_r(NULL, ":", &save), "three"));
	CHECK(!strtok_r(NULL, ":", &save));
	char *copy = strndup("abcdef", 3);
	CHECK(copy && !strcmp(copy, "abc"));
	free(copy);
	CHECK(toupper('a') == 'A' && tolower('Z') == 'z' && isspace('\t') && !isdigit('x'));
	CHECK(strerror_r(EINVAL, text, sizeof(text)) == 0 && strlen(text));
	CHECK(strerror_r(EINVAL, text, 1) == ERANGE && !text[0]);
	time_t timestamp = 951782400; /* 2000-02-29 UTC. */
	struct tm date;
	CHECK(gmtime_r(&timestamp, &date) == &date && date.tm_year == 100 &&
	      date.tm_mon == 1 && date.tm_mday == 29 && date.tm_isdst == 0);
	CHECK(timegm(&date) == timestamp);
	timestamp = -1;
	CHECK(gmtime_r(&timestamp, &date) && date.tm_year == 69 && date.tm_sec == 59);
	CHECK(timegm(&date) == -1 && gmtime(&timestamp)->tm_year == 69);
	CHECK(difftime(INT64_MAX, INT64_MAX - 1) == 1.0);
	CHECK(difftime(INT64_MIN, INT64_MAX) < -1.8e19);
	date = (struct tm){.tm_year = 100, .tm_mon = 1, .tm_mday = INT_MIN};
	CHECK(timegm(&date) != (time_t)-1);
#ifdef __HYPER__
	void *pointer = (void *)1;
	CHECK(posix_memalign(&pointer, 3, 64) == EINVAL && pointer == (void *)1);
	CHECK(posix_memalign(&pointer, 4096, 17) == 0 && !((uintptr_t)pointer & 4095));
	free(pointer);
	struct timespec before, after;
	CHECK(clock_gettime(CLOCK_MONOTONIC, &before) == 0);
	CHECK(clock_gettime(CLOCK_MONOTONIC, &after) == 0);
	CHECK(after.tv_sec > before.tv_sec ||
	      (after.tv_sec == before.tv_sec && after.tv_nsec >= before.tv_nsec));
	CHECK(clock_gettime(-1, &after) == -1 && errno == EINVAL);
	size_t page;
	CHECK(!hyper_page_size(&page) && getpagesize() == (int)page &&
	      sysconf(_SC_PAGESIZE) == (long)page);
	CHECK(!sched_yield());
	struct timespec delay = {0, 1000000};
	CHECK(!clock_gettime(CLOCK_MONOTONIC, &before));
	CHECK(!nanosleep(&delay, NULL));
	CHECK(!clock_gettime(CLOCK_MONOTONIC, &after));
	CHECK((after.tv_sec - before.tv_sec) * 1000000000 +
	      after.tv_nsec - before.tv_nsec >= 1000000);
	CHECK(!clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &after, NULL));
	CHECK(!thrd_sleep(&(struct timespec){0, 0}, NULL));
	thrd_yield();
	CHECK(!usleep(1) && sleep(0) == 0);
	delay.tv_nsec = 1000000000;
	CHECK(nanosleep(&delay, NULL) == -1 && errno == EINVAL);
	struct timeval wall;
	int wall_status = gettimeofday(&wall, NULL);
	CHECK(wall_status == 0 || (wall_status == -1 && errno == ENOTSUP));
	if (!wall_status) CHECK(wall.tv_usec >= 0 && wall.tv_usec < 1000000);
	CHECK(!getenv("=invalid") && !getenv(""));
	const hyper_startup_t *startup = hyper_runtime_startup();
	for (size_t i = 0; i < startup->environment_count; ++i) {
		char *entry = startup->environment[i], *equals = strchr(entry, '=');
		if (!equals || equals == entry) continue;
		char *name = strndup(entry, (size_t)(equals - entry));
		CHECK(name && getenv(name));
		free(name);
	}
	char *cwd = getcwd(NULL, 0);
	CHECK(cwd && cwd[0] == '/');
	char *resolved = realpath(".", NULL);
	CHECK(resolved && !strcmp(cwd, resolved));
	CHECK(!chdir(cwd));
	free(cwd); free(resolved);
	CHECK(!getcwd(text, 1) && errno == ERANGE);
	CHECK(chdir("") == -1 && errno == ENOENT);
	CHECK(!paths());
	size_t written;
	const char message[] = "MUSL-NATIVE: PASS\n";
	CHECK(!hyper_runtime_stdio_write(1, message, sizeof(message) - 1, &written));
	CHECK(written == sizeof(message) - 1);
#endif
	return 0;
}
