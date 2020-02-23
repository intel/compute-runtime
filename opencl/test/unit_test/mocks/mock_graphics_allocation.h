/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"

namespace NEO {
class MockGraphicsAllocation : public MemoryAllocation {
  public:
    using MemoryAllocation::aubInfo;
    using MemoryAllocation::MemoryAllocation;
    using MemoryAllocation::objectNotResident;
    using MemoryAllocation::objectNotUsed;
    using MemoryAllocation::usageInfos;

    MockGraphicsAllocation()
        : MemoryAllocation(0, AllocationType::UNKNOWN, nullptr, 0u, 0, MemoryPool::MemoryNull) {}

    MockGraphicsAllocation(void *buffer, size_t sizeIn)
        : MemoryAllocation(0, AllocationType::UNKNOWN, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::MemoryNull) {}

    MockGraphicsAllocation(void *buffer, uint64_t gpuAddr, size_t sizeIn)
        : MemoryAllocation(0, AllocationType::UNKNOWN, buffer, gpuAddr, 0llu, sizeIn, MemoryPool::MemoryNull) {}

    void resetInspectionIds() {
        for (auto &usageInfo : usageInfos) {
            usageInfo.inspectionId = 0u;
        }
    }

    void overrideMemoryPool(MemoryPool::Type pool) {
        this->memoryPool = pool;
    }
};
} // namespace NEO
