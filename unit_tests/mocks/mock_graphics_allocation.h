/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {
class MockGraphicsAllocation : public GraphicsAllocation {
  public:
    using GraphicsAllocation::GraphicsAllocation;
    using GraphicsAllocation::objectNotResident;
    using GraphicsAllocation::objectNotUsed;

    MockGraphicsAllocation(void *buffer, size_t sizeIn) : GraphicsAllocation(buffer, castToUint64(buffer), 0llu, sizeIn) {
    }
    void resetInspectionId() {
        this->inspectionId = 0;
    }
    void overrideMemoryPool(MemoryPool::Type pool) {
        this->memoryPool = pool;
    }
};
} // namespace OCLRT
