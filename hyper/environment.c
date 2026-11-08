/* SPDX-FileCopyrightText: 2026 roolrz
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <hyper/startup.h>

char *getenv(const char *name)
{
	if (!name || !*name || strchr(name, '=')) return NULL;
	const hyper_startup_t *startup = hyper_runtime_startup();
	if (!startup) return NULL;
	size_t length = strlen(name);
	for (size_t i = 0; i < startup->environment_count; ++i) {
		char *entry = startup->environment[i];
		if (!strncmp(entry, name, length) && entry[length] == '=')
			return entry + length + 1;
	}
	return NULL;
}
