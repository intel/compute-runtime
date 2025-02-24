/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/host_pointer_manager.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

void HostPointerManager::MapBasedAllocationTracker::insert(const HostPointerData &allocationsData) {
    allocations.insert(std::make_pair(reinterpret_cast<void *>(allocationsData.basePtr), allocationsData));
}

void HostPointerManager::MapBasedAllocationTracker::remove(const void *ptr) {
    HostPointerContainer::iterator iter;
    iter = allocations.find(ptr);
    allocations.erase(iter);
}

HostPointerData *HostPointerManager::MapBasedAllocationTracker::get(const void *ptr) {
    HostPointerContainer::iterator iter, end;
    HostPointerData *hostPtrData;
    if ((ptr == nullptr) || (allocations.size() == 0)) {
        return nullptr;
    }
    end = allocations.end();
    iter = allocations.lower_bound(ptr);
    if (((iter != end) && (iter->first != ptr)) ||
        (iter == end)) {
        if (iter == allocations.begin()) {
            iter = end;
        } else {
            iter--;
        }
    }
    if (iter != end) {
        hostPtrData = &iter->second;
        char *charPtr = reinterpret_cast<char *>(hostPtrData->basePtr);
        if (ptr < (charPtr + hostPtrData->size)) {
            return hostPtrData;
        }
    }
    return nullptr;
}

HostPointerManager::HostPointerManager(NEO::MemoryManager *memoryManager) : memoryManager(memoryManager) {
}

HostPointerManager::~HostPointerManager() {
}

ze_result_t HostPointerManager::createHostPointerMultiAllocation(std::vector<Device *> &devices, void *ptr, size_t size) {
    if (size == 0 || ptr == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    size_t endAddress = reinterpret_cast<size_t>(ptr) + size;

    std::unique_lock<NEO::SpinLock> lock(this->mtx);
    auto beginAllocation = hostPointerAllocations.get(ptr);
    auto endingAllocation = hostPointerAllocations.get(reinterpret_cast<void *>(endAddress - 1));
    if (beginAllocation != nullptr && beginAllocation == endingAllocation) {
        return ZE_RESULT_SUCCESS;
    }
    if (beginAllocation != nullptr) {
        if (endingAllocation != nullptr) {
            return ZE_RESULT_ERROR_OVERLAPPING_REGIONS;
        }
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }
    if (endingAllocation != nullptr) {
        UNRECOVERABLE_IF(endingAllocation->basePtr == ptr);
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    HostPointerData hostData(static_cast<uint32_t>(devices.size() - 1));
    hostData.basePtr = ptr;
    hostData.size = size;
    for (auto device : devices) {
        NEO::GraphicsAllocation *gfxAlloc = createHostPointerAllocation(device->getRootDeviceIndex(),
                                                                        ptr,
                                                                        size,
                                                                        device->getNEODevice()->getDeviceBitfield());
        if (gfxAlloc == nullptr) {
            auto allocations = hostData.hostPtrAllocations.getGraphicsAllocations();
            for (auto &allocation : allocations) {
                memoryManager->freeGraphicsMemory(allocation);
            }
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        hostData.hostPtrAllocations.addAllocation(gfxAlloc);
    }
    hostPointerAllocations.insert(hostData);
    return ZE_RESULT_SUCCESS;
}

NEO::GraphicsAllocation *HostPointerManager::createHostPointerAllocation(uint32_t rootDeviceIndex,
                                                                         void *ptr,
                                                                         size_t size,
                                                                         const NEO::DeviceBitfield &deviceBitfield) {
    NEO::AllocationProperties properties = {rootDeviceIndex, false, size,
                                            NEO::AllocationType::externalHostPtr,
                                            false, deviceBitfield};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = true;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties,
                                                                          ptr);
    return allocation;
}

HostPointerData *HostPointerManager::getHostPointerAllocation(const void *ptr) {
    std::unique_lock<NEO::SpinLock> lock(mtx);
    return hostPointerAllocations.get(ptr);
}

bool HostPointerManager::freeHostPointerAllocation(void *ptr) {
    std::unique_lock<NEO::SpinLock> lock(mtx);
    HostPointerData *hostPtrData = hostPointerAllocations.get(ptr);
    if (hostPtrData == nullptr) {
        return false;
    }
    auto graphicsAllocations = hostPtrData->hostPtrAllocations.getGraphicsAllocations();
    for (auto gpuAllocation : graphicsAllocations) {
        memoryManager->freeGraphicsMemory(gpuAllocation);
    }
    hostPointerAllocations.remove(hostPtrData->basePtr);
    return true;
}

} // namespace L0
