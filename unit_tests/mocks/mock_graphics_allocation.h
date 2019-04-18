/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"

namespace NEO {
class MockGraphicsAllocation : public MemoryAllocation {
  public:
    using MemoryAllocation::MemoryAllocation;
    using MemoryAllocation::objectNotResident;
    using MemoryAllocation::objectNotUsed;
    using MemoryAllocation::usageInfos;

    MockGraphicsAllocation()
        : MockGraphicsAllocation(true) {}

    MockGraphicsAllocation(bool multiOsContextCapable)
        : MemoryAllocation(AllocationType::UNKNOWN, nullptr, 0u, 0, MemoryPool::MemoryNull, multiOsContextCapable) {}

    MockGraphicsAllocation(void *buffer, size_t sizeIn)
        : MemoryAllocation(AllocationType::UNKNOWN, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::MemoryNull, false) {}

    MockGraphicsAllocation(void *buffer, uint64_t gpuAddr, size_t sizeIn)
        : MemoryAllocation(AllocationType::UNKNOWN, buffer, gpuAddr, 0llu, sizeIn, MemoryPool::MemoryNull, false) {}

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
