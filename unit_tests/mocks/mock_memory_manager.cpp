/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/surface_formats.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
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

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryForImage(const AllocationData &allocationData) {
    allocateForImageCalled = true;
    auto *allocation = MemoryManager::allocateGraphicsMemoryForImage(allocationData);
    if (redundancyRatio != 1) {
        memset((unsigned char *)allocation->getUnderlyingBuffer(), 0, allocationData.imgInfo->size * redundancyRatio);
    }
    return allocation;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemory64kb(AllocationData allocationData) {
    allocation64kbPageCreated = true;
    preferRenderCompressedFlagPassed = allocationData.flags.preferRenderCompressed;

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemory64kb(allocationData);
    if (allocation) {
        allocation->gmm = new Gmm(allocation->getUnderlyingBuffer(), allocationData.size, false, preferRenderCompressedFlagPassed, true, 0);
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
GraphicsAllocation *MockMemoryManager::allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) {
    bool allocateMemory = ptr == nullptr;
    AllocationData allocationData;
    if (allocationOrigin == AllocationOrigin::EXTERNAL_ALLOCATION) {
        getAllocationData(allocationData, MockAllocationProperties::getPropertiesFor32BitExternalAllocation(size, allocateMemory), 0u, ptr);
    } else {
        getAllocationData(allocationData, MockAllocationProperties::getPropertiesFor32BitInternalAllocation(size, allocateMemory), 0u, ptr);
    }
    return allocate32BitGraphicsMemoryImpl(allocationData);
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount) {
    this->failedAllocationsCount = failedAllocationsCount;
}

} // namespace OCLRT
