/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HYPER_STRING_SCAN_H
#define HYPER_STRING_SCAN_H

#include <stddef.h>

/* The strto* scanner only consumes a NUL-terminated string. Keeping it separate
 * from FILE avoids pulling unsupported file-descriptor stdio into this port. */
struct hyper_string_scan { const unsigned char *start, *cursor; };
#define sh_fromstring(f, s) ((f)->start = (f)->cursor = (const unsigned char *)(s))
#define shlim(f, n) ((void)(n), (f)->cursor = (f)->start)
#define shcnt(f) ((f)->cursor - (f)->start)
#define shgetc(f) (*(f)->cursor++)
#define shunget(f) ((void)--(f)->cursor)

#endif
