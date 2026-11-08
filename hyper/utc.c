/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <time.h>
/* UTC conversion does not require the unsupported timezone/locale loader. */
const char __utc[] = "UTC";

double difftime(time_t end, time_t start)
{
	/* The signed difference may overflow time_t even though double can hold
	 * it. Subtract unsigned magnitudes before converting, preserving nearby
	 * values at the extremes instead of rounding each timestamp first. */
	return end >= start ? (double)((uint64_t)end - (uint64_t)start) :
		-(double)((uint64_t)start - (uint64_t)end);
}
