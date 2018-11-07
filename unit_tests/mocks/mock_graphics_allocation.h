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

    MockGraphicsAllocation() : MockGraphicsAllocation(true) {}
    MockGraphicsAllocation(bool shareable) : GraphicsAllocation(nullptr, 0u, 0, maxOsContextCount, shareable) {}
    MockGraphicsAllocation(void *buffer, size_t sizeIn) : GraphicsAllocation(buffer, castToUint64(buffer), 0llu, sizeIn, maxOsContextCount, false) {
    }
    void resetInspectionId() {
        this->inspectionId = 0;
    }
    void overrideMemoryPool(MemoryPool::Type pool) {
        this->memoryPool = pool;
    }
};
} // namespace OCLRT
