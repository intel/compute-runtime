/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"

#include "shared/source/memory_manager/host_ptr_defines.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {

WddmMemoryOperationsHandler::WddmMemoryOperationsHandler(Wddm *wddm) : wddm(wddm) {
    residentAllocations = std::make_unique<WddmResidentAllocationsContainer>(wddm);
}

MemoryOperationsStatus WddmMemoryOperationsHandler::makeResident(ArrayRef<GraphicsAllocation *> gfxAllocations) {
    uint32_t totalHandlesCount = 0;
    constexpr uint32_t stackAllocations = 64;
    constexpr uint32_t stackHandlesCount = NEO::maxFragmentsCount * EngineLimits::maxHandleCount * stackAllocations;
    StackVec<D3DKMT_HANDLE, stackHandlesCount> handlesForResidency;

    for (const auto &allocation : gfxAllocations) {
        WddmAllocation *wddmAllocation = reinterpret_cast<WddmAllocation *>(allocation);

        if (wddmAllocation->fragmentsStorage.fragmentCount > 0) {
            for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                handlesForResidency[totalHandlesCount++] = wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle;
            }
        } else {
            memcpy_s(&handlesForResidency[totalHandlesCount],
                     wddmAllocation->getNumHandles() * sizeof(D3DKMT_HANDLE),
                     wddmAllocation->getHandles().data(),
                     wddmAllocation->getNumHandles() * sizeof(D3DKMT_HANDLE));
            totalHandlesCount += wddmAllocation->getNumHandles();
        }
    }
    return residentAllocations->makeResidentResources(handlesForResidency.begin(), totalHandlesCount);
}

MemoryOperationsStatus WddmMemoryOperationsHandler::evict(GraphicsAllocation &gfxAllocation) {
    constexpr uint32_t stackHandlesCount = NEO::maxFragmentsCount * EngineLimits::maxHandleCount;
    StackVec<D3DKMT_HANDLE, stackHandlesCount> handlesForEviction;
    WddmAllocation &wddmAllocation = reinterpret_cast<WddmAllocation &>(gfxAllocation);
    uint32_t totalHandleCount = 0;
    if (wddmAllocation.fragmentsStorage.fragmentCount > 0) {
        OsHandleStorage &fragmentStorage = wddmAllocation.fragmentsStorage;

        for (uint32_t allocId = 0; allocId < fragmentStorage.fragmentCount; allocId++) {
            handlesForEviction.push_back(fragmentStorage.fragmentStorageData[allocId].osHandleStorage->handle);
            totalHandleCount++;
        }
    } else {
        const D3DKMT_HANDLE *handlePtr = wddmAllocation.getHandles().data();
        size_t handleCount = wddmAllocation.getNumHandles();
        for (uint32_t i = 0; i < handleCount; i++, totalHandleCount++) {
            handlesForEviction.push_back(*handlePtr);
            handlePtr++;
        }
    }
    return residentAllocations->evictResources(handlesForEviction.begin(), totalHandleCount);
}

MemoryOperationsStatus WddmMemoryOperationsHandler::isResident(GraphicsAllocation &gfxAllocation) {
    WddmAllocation &wddmAllocation = reinterpret_cast<WddmAllocation &>(gfxAllocation);
    D3DKMT_HANDLE defaultHandle = 0u;
    if (wddmAllocation.fragmentsStorage.fragmentCount > 0) {
        defaultHandle = wddmAllocation.fragmentsStorage.fragmentStorageData[0].osHandleStorage->handle;
    } else {
        defaultHandle = wddmAllocation.getDefaultHandle();
    }
    return residentAllocations->isAllocationResident(defaultHandle);
}

} // namespace NEO
