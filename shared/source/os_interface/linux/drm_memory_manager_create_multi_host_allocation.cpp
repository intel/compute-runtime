/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"

namespace NEO {

DrmAllocation *DrmMemoryManager::createMultiHostAllocation(const AllocationData &allocationData) {
    if (!isAligned<MemoryConstants::pageSize>(allocationData.size)) {
        return nullptr;
    }
    auto numTiles = allocationData.storageInfo.getNumBanks();
    auto sizePerTile = allocationData.size;
    auto hostSizeToAllocate = numTiles * sizePerTile;

    auto cpuBasePointer = alignedMallocWrapper(hostSizeToAllocate, MemoryConstants::pageSize);
    if (!cpuBasePointer) {
        return nullptr;
    }

    zeroCpuMemoryIfRequested(allocationData, cpuBasePointer, hostSizeToAllocate);

    auto gpuAddress = allocationData.gpuAddress;
    bool addressReserved = false;
    if (gpuAddress == 0) {
        gpuAddress = acquireGpuRange(sizePerTile, allocationData.rootDeviceIndex, HeapIndex::HEAP_STANDARD);
        addressReserved = true;
    } else {
        gpuAddress = allocationData.gpuAddress;
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
    auto allocation = new DrmAllocation(allocationData.rootDeviceIndex, numTiles, allocationData.type,
                                        nullptr /*bo*/, cpuBasePointer, canonizedGpuAddress, sizePerTile, MemoryPool::System4KBPages);

    allocation->storageInfo = allocationData.storageInfo;
    allocation->setFlushL3Required(true);
    allocation->setUncacheable(true);
    allocation->setDriverAllocatedCpuPtr(cpuBasePointer);

    if (addressReserved) {
        allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), sizePerTile);
    }

    for (auto tile = 0u, currentBank = 0u; tile < numTiles; ++tile, ++currentBank) {
        while (!allocationData.storageInfo.memoryBanks.test(currentBank)) {
            ++currentBank;
        }

        auto boHostPtr = static_cast<uint8_t *>(cpuBasePointer) + tile * sizePerTile;
        auto bo = allocUserptr(reinterpret_cast<uintptr_t>(boHostPtr), sizePerTile, allocationData.rootDeviceIndex);
        if (!bo) {
            freeGraphicsMemoryImpl(allocation);
            return nullptr;
        }

        bo->setAddress(gpuAddress);
        allocation->getBufferObjectToModify(currentBank) = bo;
    }

    return allocation;
}

} // namespace NEO
