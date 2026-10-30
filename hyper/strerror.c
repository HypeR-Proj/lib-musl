/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 * Error messages are the unmodified musl table (see ../COPYRIGHT).
 */
#include <errno.h>
#include <string.h>

/* This port supports only the C locale. No locale-selection API is exported. */
char *strerror(int error)
{
	switch (error) {
#define E(number, text) case number: return (char *)text;
#include "../src/errno/__strerror.h"
#undef E
	default: return "No error information";
	}
}
