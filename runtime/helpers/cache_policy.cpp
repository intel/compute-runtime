/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/cache_policy.h"

#include "runtime/helpers/aligned_memory.h"

bool isL3Capable(void *ptr, size_t size) {
    if (alignUp(ptr, MemoryConstants::cacheLineSize) == ptr &&
        alignUp(size, MemoryConstants::cacheLineSize) == size) {
        return true;
    }
    return false;
}