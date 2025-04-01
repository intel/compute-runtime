/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"

#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/host_ptr_defines.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"

namespace NEO {

WddmMemoryOperationsHandler::WddmMemoryOperationsHandler(Wddm *wddm) : wddm(wddm) {
    residentAllocations = std::make_unique<WddmResidentAllocationsContainer>(wddm, false);
}

WddmMemoryOperationsHandler::~WddmMemoryOperationsHandler() = default;

MemoryOperationsStatus WddmMemoryOperationsHandler::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) {
    uint32_t totalHandlesCount = 0;
    constexpr uint32_t stackAllocations = 64;
    constexpr uint32_t stackHandlesCount = NEO::maxFragmentsCount * EngineLimits::maxHandleCount * stackAllocations;
    StackVec<D3DKMT_HANDLE, stackHandlesCount> handlesForResidency;
    size_t totalSize = 0;

    for (const auto &allocation : gfxAllocations) {
        allocation->setExplicitlyMadeResident(true);
        WddmAllocation *wddmAllocation = reinterpret_cast<WddmAllocation *>(allocation);
        totalSize += wddmAllocation->getAlignedSize();

        if (wddmAllocation->fragmentsStorage.fragmentCount > 0) {
            for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                handlesForResidency[totalHandlesCount++] =
                    static_cast<OsHandleWin *>(wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage)->handle;
            }
        } else {
            memcpy_s(&handlesForResidency[totalHandlesCount],
                     wddmAllocation->getNumGmms() * sizeof(D3DKMT_HANDLE),
                     &wddmAllocation->getHandles()[0],
                     wddmAllocation->getNumGmms() * sizeof(D3DKMT_HANDLE));
            totalHandlesCount += wddmAllocation->getNumGmms();
        }
    }
    return residentAllocations->makeResidentResources(handlesForResidency.begin(), totalHandlesCount, totalSize, forcePagingFence);
}

MemoryOperationsStatus WddmMemoryOperationsHandler::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    gfxAllocation.setExplicitlyMadeResident(false);
    constexpr uint32_t stackHandlesCount = NEO::maxFragmentsCount * EngineLimits::maxHandleCount;
    StackVec<D3DKMT_HANDLE, stackHandlesCount> handlesForEviction;
    WddmAllocation &wddmAllocation = reinterpret_cast<WddmAllocation &>(gfxAllocation);
    uint32_t totalHandleCount = 0;
    if (wddmAllocation.fragmentsStorage.fragmentCount > 0) {
        OsHandleStorage &fragmentStorage = wddmAllocation.fragmentsStorage;

        for (uint32_t allocId = 0; allocId < fragmentStorage.fragmentCount; allocId++) {
            handlesForEviction.push_back(static_cast<OsHandleWin *>(fragmentStorage.fragmentStorageData[allocId].osHandleStorage)->handle);
            totalHandleCount++;
        }
    } else {
        const D3DKMT_HANDLE *handlePtr = &wddmAllocation.getHandles()[0];
        size_t handleCount = wddmAllocation.getNumGmms();
        for (uint32_t i = 0; i < handleCount; i++, totalHandleCount++) {
            handlesForEviction.push_back(*handlePtr);
            handlePtr++;
        }
    }
    return residentAllocations->evictResources(handlesForEviction.begin(), totalHandleCount);
}

MemoryOperationsStatus WddmMemoryOperationsHandler::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
    WddmAllocation &wddmAllocation = reinterpret_cast<WddmAllocation &>(gfxAllocation);
    D3DKMT_HANDLE defaultHandle = 0u;
    if (wddmAllocation.fragmentsStorage.fragmentCount > 0) {
        defaultHandle = static_cast<OsHandleWin *>(wddmAllocation.fragmentsStorage.fragmentStorageData[0].osHandleStorage)->handle;
    } else {
        defaultHandle = wddmAllocation.getDefaultHandle();
    }
    return residentAllocations->isAllocationResident(defaultHandle);
}

MemoryOperationsStatus WddmMemoryOperationsHandler::free(Device *device, GraphicsAllocation &gfxAllocation) {
    if (gfxAllocation.isExplicitlyMadeResident()) {
        WddmAllocation &wddmAllocation = reinterpret_cast<WddmAllocation &>(gfxAllocation);

        if (wddmAllocation.fragmentsStorage.fragmentCount > 0) {
            OsHandleStorage &fragmentStorage = wddmAllocation.fragmentsStorage;

            for (uint32_t allocId = 0; allocId < fragmentStorage.fragmentCount; allocId++) {
                residentAllocations->removeResource(static_cast<OsHandleWin *>(fragmentStorage.fragmentStorageData[allocId].osHandleStorage)->handle);
            }
        } else {
            const auto &handles = wddmAllocation.getHandles();
            size_t handleCount = wddmAllocation.getNumGmms();
            for (uint32_t i = 0; i < handleCount; i++) {
                residentAllocations->removeResource(handles[i]);
            }
        }
    }
    return MemoryOperationsStatus::success;
}
} // namespace NEO
