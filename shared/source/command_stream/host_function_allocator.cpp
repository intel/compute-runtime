/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_allocator.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <cstring>
#include <limits>

namespace NEO {

HostFunctionAllocator::HostFunctionAllocator(MemoryManager *memoryManager,
                                             CommandStreamReceiver *csr,
                                             size_t storageSize,
                                             uint32_t rootDeviceIndex,
                                             uint32_t maxRootDeviceIndex,
                                             uint32_t activePartitions,
                                             bool isTbx)
    : memoryManager(memoryManager),
      csr(csr),
      storageSize(storageSize),
      rootDeviceIndex(rootDeviceIndex),
      maxRootDeviceIndex(maxRootDeviceIndex),
      activePartitions(activePartitions),
      isTbx(isTbx) {
}

HostFunctionAllocator::~HostFunctionAllocator() {
    if (storage) {
        for (auto *allocation : storage->getGraphicsAllocations()) {
            memoryManager->freeGraphicsMemory(allocation);
        }
    }
    currentOffset = 0u;
}

HostFunctionChunk HostFunctionAllocator::obtainChunk() {

    std::lock_guard<std::mutex> lock(allocationMutex);

    const size_t size = partitionOffset * activePartitions;
    const size_t requiredSize = alignUp(size, chunkAlignment);

    UNRECOVERABLE_IF(currentOffset + requiredSize > storageSize);

    if (storage == nullptr) {
        allocateStorage();
    }

    auto *allocation = storage->getGraphicsAllocation(rootDeviceIndex);
    auto *cpuBase = reinterpret_cast<uint8_t *>(allocation->getUnderlyingBuffer());

    HostFunctionChunk chunk{};
    chunk.allocation = allocation;
    chunk.cpuPtr = cpuBase + currentOffset;

    currentOffset += requiredSize;

    return chunk;
}

void HostFunctionAllocator::allocateStorage() {

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);

    auto multiAllocation = std::make_unique<MultiGraphicsAllocation>(maxRootDeviceIndex);
    AllocationProperties properties{rootDeviceIndex, storageSize, AllocationType::hostFunction, systemMemoryBitfield};

    void *baseCpuAddress = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, *multiAllocation);

    std::memset(baseCpuAddress, 0, storageSize);

    if (isTbx) {
        constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();
        auto lock = csr->obtainHostAllocationLock();
        for (auto *ga : multiAllocation->getGraphicsAllocations()) {
            ga->setTbxWritable(true, allBanks);
            csr->writeMemory(*ga, true, 0, static_cast<uint32_t>(storageSize));
            ga->setTbxWritable(false, allBanks);
        }
    }

    this->storage = std::move(multiAllocation);
    currentOffset = 0u;
}

} // namespace NEO
