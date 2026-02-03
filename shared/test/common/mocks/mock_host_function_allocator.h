/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/host_function/host_function_allocator.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include <cstring>

namespace NEO {
class MemoryManager;
class CommandStreamReceiver;

class MockHostFunctionAllocator : public HostFunctionAllocator {
  public:
    MockHostFunctionAllocator(MemoryManager *memoryManager,
                              CommandStreamReceiver *csr,
                              size_t storageSize,
                              uint32_t rootDeviceIndex,
                              uint32_t maxRootDeviceIndex,
                              uint32_t activePartitions,
                              bool isTbx)
        : HostFunctionAllocator(memoryManager, csr, storageSize, rootDeviceIndex, maxRootDeviceIndex, activePartitions, isTbx) {
    }

    ~MockHostFunctionAllocator() override {

        alignedFree(cpuStorage);
    }

    HostFunctionChunk obtainChunk() override {

        const size_t size = partitionOffset * activePartitions;
        const size_t requiredSize = alignUp(size, chunkAlignment);

        if (cpuStorage == nullptr) {
            allocateStorage();
        }

        auto *currentAllocation = allocation.get();
        auto *basePtr = reinterpret_cast<uint8_t *>(cpuStorage);
        auto *cpuPtr = basePtr + currentOffset;

        if (cpuPtr != nullptr) {
            std::memset(cpuPtr, 0, requiredSize);
        }

        HostFunctionChunk chunk{};
        chunk.allocation = currentAllocation;
        chunk.cpuPtr = cpuPtr;

        currentOffset += requiredSize;

        return chunk;
    }

    void allocateStorage() {

        const size_t allocSize = storageSize;

        cpuStorage = alignedMalloc(allocSize, MemoryConstants::cacheLineSize);
        if (cpuStorage == nullptr) {
            storageSize = 0u;
            return;
        }

        std::memset(cpuStorage, 0, allocSize);

        allocation = std::make_unique<MockGraphicsAllocation>(rootDeviceIndex, cpuStorage, allocSize);
        allocation->allocationType = AllocationType::hostFunction;

        currentOffset = 0u;
    }

    std::unique_ptr<MockGraphicsAllocation> allocation = nullptr;
    void *cpuStorage = nullptr;
    size_t currentOffset = 0u;
};

} // namespace NEO
