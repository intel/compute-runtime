/*
 * Copyright (C) 2017-2019 Intel Corporation
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
    using GraphicsAllocation::usageInfos;

    MockGraphicsAllocation() : MockGraphicsAllocation(true) {}
    MockGraphicsAllocation(bool multiOsContextCapable) : GraphicsAllocation(nullptr, 0u, 0, multiOsContextCapable) {}
    MockGraphicsAllocation(void *buffer, size_t sizeIn) : GraphicsAllocation(buffer, castToUint64(buffer), 0llu, sizeIn, false) {}
    MockGraphicsAllocation(void *buffer, uint64_t gpuAddr, size_t sizeIn) : GraphicsAllocation(buffer, gpuAddr, 0llu, sizeIn, false) {}

    void resetInspectionIds() {
        for (auto &usageInfo : usageInfos) {
            usageInfo.inspectionId = 0u;
        }
    }

    void overrideMemoryPool(MemoryPool::Type pool) {
        this->memoryPool = pool;
    }
};
} // namespace OCLRT
