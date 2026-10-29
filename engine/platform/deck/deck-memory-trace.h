// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
enum {PM_MEMORY_NONE,PM_MEMORY_ISOLATE,PM_MEMORY_SEAL,PM_MEMORY_VERIFY,PM_MEMORY_READY};
typedef struct {
 uint32_t stage;
 int32_t nativeResult,kernelResult,observedProtection,error;
} PmDeckMemoryTrace;
