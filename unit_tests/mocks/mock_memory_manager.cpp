/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_memory_manager.h"

#include "core/gmm_helper/gmm.h"
#include "core/memory_manager/deferred_deleter.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/surface_formats.h"
#include "unit_tests/mocks/mock_allocation_properties.h"

#include <cstring>

namespace NEO {

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
    if (failAllocateSystemMemory) {
        return nullptr;
    }
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

GraphicsAllocation *MockMemoryManager::allocateShareableMemory(const AllocationData &allocationData) {
    allocateForShareableCalled = true;
    return OsAgnosticMemoryManager::allocateShareableMemory(allocationData);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    allocation64kbPageCreated = true;
    preferRenderCompressedFlagPassed = allocationData.flags.preferRenderCompressed;

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemory64kb(allocationData);
    if (allocation) {
        allocation->setDefaultGmm(new Gmm(executionEnvironment.getGmmClientContext(), allocation->getUnderlyingBuffer(), allocationData.size, false, preferRenderCompressedFlagPassed, true, {}));
        allocation->getDefaultGmm()->isRenderCompressed = preferRenderCompressedFlagPassed;
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
        if (localMemorySupported) {
            static_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::LocalMemory);
        }
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

GraphicsAllocation *MockMemoryManager::allocate32BitGraphicsMemory(size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType) {
    bool allocateMemory = ptr == nullptr;
    AllocationData allocationData;
    MockAllocationProperties properties(allocateMemory, size, allocationType);
    getAllocationData(allocationData, properties, ptr, createStorageInfoFromProperties(properties));
    return allocate32BitGraphicsMemoryImpl(allocationData);
}

GraphicsAllocation *MockMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    if (failAllocate32Bit) {
        return nullptr;
    }
    return OsAgnosticMemoryManager::allocate32BitGraphicsMemoryImpl(allocationData);
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {
    this->failedAllocationsCount = failedAllocationsCount;
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment, bool enableLocalMemory)
    : MockMemoryManager(enableLocalMemory, executionEnvironment) {
    this->failedAllocationsCount = failedAllocationsCount;
}

} // namespace NEO
