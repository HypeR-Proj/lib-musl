#ifndef INTSCAN_H
#define INTSCAN_H

#include <features.h>
#ifdef HYPER_MUSL_STRING_ONLY
#include "string_scan.h"

hidden unsigned long long __intscan(struct hyper_string_scan *, unsigned, int, unsigned long long);
#else
#include <stdio.h>
hidden unsigned long long __intscan(FILE *, unsigned, int, unsigned long long);
#endif

#endif
