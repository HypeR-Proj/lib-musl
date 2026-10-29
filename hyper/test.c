/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __HYPER__
#include <hyper/stream.h>
#include <hyper/std.h>
#endif

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static int compare(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

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
	size_t written;
	const char message[] = "MUSL-NATIVE: PASS\n";
	CHECK(!hyper_runtime_stdio_write(1, message, sizeof(message) - 1, &written));
	CHECK(written == sizeof(message) - 1);
#endif
	return 0;
}
