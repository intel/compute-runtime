/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"

#include "shared/source/utilities/logger.h"

void logAlignedMalloc(const char *function, void *originalPtr, size_t size) {
    DBG_LOG(LogAlignedAllocations, function, "Pointer:", originalPtr, "size:", size);
}

void logAlignedFree(const char *function, void *originalPtr) {
    DBG_LOG(LogAlignedAllocations, function, "Pointer:", originalPtr);
}
