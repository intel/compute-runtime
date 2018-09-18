/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "graphics_allocation.h"
#include "runtime/helpers/aligned_memory.h"

bool OCLRT::GraphicsAllocation::isL3Capable() {
    auto ptr = ptrOffset(cpuPtr, static_cast<size_t>(this->allocationOffset));
    if (alignUp(ptr, MemoryConstants::cacheLineSize) == ptr && alignUp(this->size, MemoryConstants::cacheLineSize) == this->size) {
        return true;
    }
    return false;
}
