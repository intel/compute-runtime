/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/surface_formats.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include <cstring>

namespace OCLRT {

void MockMemoryManager::setDeferredDeleter(DeferredDeleter *deleter) {
    deferredDeleter.reset(deleter);
}

void MockMemoryManager::overrideAsyncDeleterFlag(bool newValue) {
    asyncDeleterEnabled = newValue;
    if (asyncDeleterEnabled && deferredDeleter == nullptr) {
        deferredDeleter = createDeferredDeleter();
    }
}

void *MockMemoryManager::allocateSystemMemory(size_t size, size_t alignment) {
    return OsAgnosticMemoryManager::allocateSystemMemory(redundancyRatio * size, alignment);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) {
    AllocationProperties adjustedProperties(properties);
    adjustedProperties.size = redundancyRatio * properties.size;
    return OsAgnosticMemoryManager::allocateGraphicsMemoryWithProperties(adjustedProperties);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryForImage(ImageInfo &imgInfo, const void *hostPtr) {
    auto *allocation = OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, hostPtr);
    if (redundancyRatio != 1) {
        memset((unsigned char *)allocation->getUnderlyingBuffer(), 0, imgInfo.size * redundancyRatio);
    }
    return allocation;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemory64kb(AllocationData allocationData) {
    allocation64kbPageCreated = true;
    preferRenderCompressedFlagPassed = allocationData.flags.preferRenderCompressed;

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemory64kb(allocationData);
    if (allocation) {
        allocation->gmm = new Gmm(allocation->getUnderlyingBuffer(), allocationData.size, false, preferRenderCompressedFlagPassed, true);
        allocation->gmm->isRenderCompressed = preferRenderCompressedFlagPassed;
    }
    return allocation;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    if (failInDevicePool) {
        status = AllocationStatus::RetryInNonDevicePool;
        return nullptr;
    }
    if (failInDevicePoolWithError) {
        status = AllocationStatus::Error;
        return nullptr;
    }

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(allocationData, status);
    if (allocation) {
        allocationInDevicePoolCreated = true;
    }
    return allocation;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    if (failInAllocateWithSizeAndAlignment) {
        return nullptr;
    }
    allocationCreated = true;
    return OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(allocationData);
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount) {
    this->failedAllocationsCount = failedAllocationsCount;
}

} // namespace OCLRT
