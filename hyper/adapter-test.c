/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include <hyper/startup.h>
#include <hyper/syscall.h>
#include <hyper/system.h>

#define CHECK(c) do { if (!(c)) { printf("adapter-test:%d: %s\n", __LINE__, #c); return 1; } } while (0)
static int c_errno;
int *__errno_location(void) { return &c_errno; }
static char *environment[] = {"A=first", "AB=second", "EMPTY=", "MALFORMED", NULL};
static hyper_startup_t startup = {.environment_count = 4, .environment = environment};
static int have_startup = 1;
const hyper_startup_t *hyper_runtime_startup(void) { return have_startup ? &startup : NULL; }
static int64_t clock_status, sleep_status, page_status, yield_status, acquire_status, operation_status;
static uint64_t now = 100, deadline, page = 4096;
static unsigned sleeps, yields, closes, acquisitions, changes;
static const char *canonical_path = "/tmp/example";
static const char *link_target = "../destination";
static uint32_t remove_option;
static uint64_t resized;
static int corrupt_path, reject_absolute;
static hyper_call_result_t result(int64_t status, uint64_t value) { return (hyper_call_result_t){status, value, 0}; }
hyper_call_result_t hyper_clock_get_monotonic(void) { return result(clock_status, now); }
hyper_native_status_t hyper_thread_sleep(uint64_t until) { deadline = until; ++sleeps; now += 100; return sleep_status; }
hyper_native_status_t hyper_thread_yield(void) { ++yields; return yield_status; }
hyper_native_status_t hyper_page_size(size_t *out) { *out = page; return page_status; }
int clock_gettime(clockid_t id, struct timespec *out)
{
    if (id != CLOCK_REALTIME || clock_status) { errno = ENOTSUP; return -1; }
    *out = (struct timespec){123, 987654321}; return 0;
}
hyper_native_status_t hyper_runtime_directory_acquire(const char *path, size_t size, hyper_native_handle_t *out)
{
    (void)size; ++acquisitions;
    *out = path[0] == '/' ? 44 : 42;
    return path[0] == '/' && reject_absolute ? HYPER_NATIVE_STATUS_ACCESS_DENIED : acquire_status;
}
hyper_native_status_t hyper_runtime_directory_change(const char *path, size_t size)
{ (void)path; (void)size; ++changes; return operation_status; }
hyper_native_status_t hyper_handle_close(hyper_native_handle_t handle)
{ if (handle != 42 && handle != 43 && handle != 44) abort(); ++closes; return 0; }
hyper_call_result_t hyper_directory_canonicalize(hyper_native_handle_t dir, const void *path, size_t size, void *out, size_t capacity)
{
    (void)dir; (void)path; (void)size;
    if (operation_status) return result(operation_status, 0);
    size_t length = strlen(canonical_path);
    if (capacity < length) return result(HYPER_NATIVE_STATUS_BUFFER_TOO_SMALL, 0);
    memcpy(out, canonical_path, length);
    return result(0, corrupt_path ? capacity + 1 : length);
}
hyper_call_result_t hyper_directory_read_link(hyper_native_handle_t dir, const void *path, size_t size, void *out, size_t capacity)
{
    (void)dir; (void)path; (void)size;
    if (operation_status) return result(operation_status, 0);
    size_t length = strlen(link_target);
    if (capacity < length) return result(HYPER_NATIVE_STATUS_BUFFER_TOO_SMALL, 0);
    memcpy(out, link_target, length); return result(0, length);
}
hyper_call_result_t hyper_directory_remove(hyper_native_handle_t dir, const void *path, size_t size, uint32_t options)
{ (void)dir; (void)path; (void)size; remove_option = options; return result(operation_status, 0); }
hyper_call_result_t hyper_directory_rename(hyper_native_handle_t dir, const void *from, size_t n, hyper_native_handle_t other, const void *to, size_t m)
{
    (void)n; (void)m;
    if (dir != (((const char *)from)[0] == '/' ? 44u : 42u) ||
        other != (((const char *)to)[0] == '/' ? 44u : 42u)) abort();
    return result(operation_status, 0);
}
hyper_call_result_t hyper_directory_link(hyper_native_handle_t dir, const void *from, size_t n, hyper_native_handle_t other, const void *to, size_t m)
{ return hyper_directory_rename(dir, from, n, other, to, m); }
hyper_call_result_t hyper_directory_symlink(hyper_native_handle_t dir, const void *path, size_t n, const void *target, size_t m)
{ (void)dir; (void)path; (void)n; (void)target; (void)m; return result(operation_status, 0); }
hyper_call_result_t hyper_directory_open_file(hyper_native_handle_t dir, const void *path, size_t n, uint64_t rights)
{ (void)dir; (void)path; (void)n; if (rights != HYPER_NATIVE_RIGHT_WRITE) abort(); return result(operation_status, 43); }
hyper_call_result_t hyper_file_resize(hyper_native_handle_t file, uint64_t size)
{ if (file != 43) abort(); resized = size; return result(operation_status, 0); }

int main(void)
{
    errno = 73;
    CHECK(!strcmp(getenv("A"), "first") && !strcmp(getenv("AB"), "second"));
    CHECK(!strcmp(getenv("EMPTY"), "") && !getenv("MALFORMED"));
    CHECK(!getenv("A=") && !getenv("") && !getenv("ABSENT") && errno == 73);
    have_startup = 0; CHECK(!getenv("A")); have_startup = 1;
    CHECK(getpagesize() == 4096 && sysconf(_SC_PAGESIZE) == 4096);
    CHECK(sysconf(_SC_CLK_TCK) == -1 && errno == EINVAL);
    page = (uint64_t)INT_MAX + 1; CHECK(getpagesize() == -1 && errno == EOVERFLOW); page = 4096;
    page_status = HYPER_NATIVE_STATUS_NOT_SUPPORTED; CHECK(getpagesize() == -1 && errno == ENOTSUP); page_status = 0;
    yield_status = HYPER_NATIVE_STATUS_NO_MEMORY; CHECK(sched_yield() == -1 && errno == ENOMEM);
    yield_status = 0; CHECK(sched_yield() == 0); thrd_yield(); CHECK(yields == 3);
    struct timeval tv = {7, 8};
    CHECK(!gettimeofday(&tv, NULL) && tv.tv_sec == 123 && tv.tv_usec == 987654);
    clock_status = HYPER_NATIVE_STATUS_NOT_SUPPORTED;
    CHECK(gettimeofday(&tv, NULL) == -1 && errno == ENOTSUP && tv.tv_usec == 987654);
    clock_status = 0;
    struct timespec req = {0, 500}, rem = {99, 99};
    errno = 73; CHECK(!clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem));
    CHECK(deadline == 600 && sleeps == 1 && rem.tv_sec == 99 && errno == 73);
    req.tv_nsec = 1000000000;
    CHECK(clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem) == EINVAL && errno == 73 && sleeps == 1);
    CHECK(nanosleep(&req, &rem) == -1 && errno == EINVAL);
    req = (struct timespec){INT64_MAX, 0}; CHECK(clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL) == EOVERFLOW);
    req = (struct timespec){0, 500}; now = UINT64_MAX - 100;
    CHECK(clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL) == EOVERFLOW); now = 100;
    CHECK(clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &req, &rem) == ENOTSUP);
    CHECK(clock_nanosleep(CLOCK_PROCESS_CPUTIME_ID, 0, &req, NULL) == EINVAL);
    req.tv_nsec = 50; CHECK(!clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, &rem) && sleeps == 1);
    req.tv_nsec = 500; sleep_status = HYPER_NATIVE_STATUS_CANCELLED;
    errno = 73; CHECK(clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem) == EINTR && rem.tv_nsec == 400 && errno == 73);
    rem = (struct timespec){99, 99}; CHECK(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, &rem) == EINTR && rem.tv_sec == 99);
    req = (struct timespec){0, 500}; CHECK(thrd_sleep(&req, &rem) == -1);
    sleep_status = HYPER_NATIVE_STATUS_NO_MEMORY; CHECK(thrd_sleep(&req, &rem) == -2);
    sleep_status = HYPER_NATIVE_STATUS_CANCELLED; CHECK(sleep(2) == 2);
    sleep_status = 0; CHECK(!usleep(1)); CHECK(!sleep(0));
    char buffer[64];
    CHECK(getcwd(buffer, sizeof(buffer)) == buffer && !strcmp(buffer, canonical_path));
    CHECK(getcwd(buffer, 2) == NULL && errno == ERANGE);
    CHECK(getcwd(buffer, 0) == NULL && errno == EINVAL);
    char *allocated = getcwd(NULL, 0); CHECK(allocated && !strcmp(allocated, canonical_path)); free(allocated);
    CHECK(realpath(".", buffer) == buffer && !strcmp(buffer, canonical_path));
    allocated = realpath(".", NULL); CHECK(allocated); free(allocated);
    corrupt_path = 1; CHECK(!getcwd(buffer, sizeof(buffer)) && errno == EIO); corrupt_path = 0;
    CHECK(!chdir("/tmp") && changes == 1); CHECK(chdir("") == -1 && errno == ENOENT && changes == 1);
    memset(buffer, '!', sizeof(buffer)); CHECK(readlink("x", buffer, 3) == 3 && !memcmp(buffer, "../!", 4));
    CHECK(readlink("x", buffer, 0) == -1 && errno == EINVAL);
    operation_status = HYPER_NATIVE_STATUS_NOT_FOUND;
    CHECK(readlink("x", buffer, sizeof(buffer)) == -1 && errno == ENOENT && buffer[3] == '!');
    CHECK(unlink("missing") == -1 && errno == ENOENT);
    operation_status = HYPER_NATIVE_STATUS_READ_ONLY; CHECK(rmdir("d") == -1 && errno == EROFS && remove_option == 1);
    operation_status = 0; CHECK(!unlink("x") && remove_option == 0);
    unsigned before = acquisitions; CHECK(!rename("relative", "other") && acquisitions == before + 1);
    before = acquisitions; CHECK(!rename("/absolute", "/other") && acquisitions == before + 1);
    before = acquisitions; CHECK(!rename("relative", "/absolute") && acquisitions == before + 2);
    before = acquisitions; CHECK(!rename("/absolute", "relative") && acquisitions == before + 2);
    reject_absolute = 1; before = closes;
    CHECK(rename("relative", "/absolute") == -1 && errno == EACCES && closes == before + 1);
    reject_absolute = 0;
    CHECK(!link("a", "b") && !symlink("../a", "b"));
    CHECK(truncate("x", -1) == -1 && errno == EINVAL);
    before = closes; CHECK(!truncate("x", 123) && resized == 123 && closes == before + 2);
    acquire_status = HYPER_NATIVE_STATUS_ACCESS_DENIED; before = closes;
    CHECK(unlink("x") == -1 && errno == EACCES && closes == before);
    acquire_status = 0;
    char longpath[4098]; memset(longpath, 'a', sizeof(longpath)); longpath[4097] = 0;
    CHECK(chdir(longpath) == -1 && errno == ENAMETOOLONG);
    puts("Native adapters: PASS");
    return 0;
}
