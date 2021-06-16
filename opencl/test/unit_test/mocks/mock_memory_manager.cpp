/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/memory_manager/deferred_deleter.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"

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
    if (isMockHostMemoryManager) {
        allocateGraphicsMemoryWithPropertiesCount++;
        if (forceFailureInPrimaryAllocation) {
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }

    recentlyPassedDeviceBitfield = properties.subDevicesBitfield;
    AllocationProperties adjustedProperties(properties);
    adjustedProperties.size = redundancyRatio * properties.size;
    return OsAgnosticMemoryManager::allocateGraphicsMemoryWithProperties(adjustedProperties);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) {
    if (returnFakeAllocation) {
        return new GraphicsAllocation(properties.rootDeviceIndex, properties.allocationType, reinterpret_cast<void *>(dummyAddress), reinterpret_cast<uint64_t>(ptr), properties.size, 0, MemoryPool::System4KBPages, maxOsContextCount);
    }
    if (isMockHostMemoryManager) {
        allocateGraphicsMemoryWithPropertiesCount++;
        if (forceFailureInAllocationWithHostPointer) {
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties, ptr);
    }
    return OsAgnosticMemoryManager::allocateGraphicsMemoryWithProperties(properties, ptr);
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
    preferRenderCompressedFlagPassed = forceRenderCompressed ? true : allocationData.flags.preferRenderCompressed;

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemory64kb(allocationData);
    if (allocation) {
        allocation->getDefaultGmm()->isCompressionEnabled = preferRenderCompressedFlagPassed;
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
    if (successAllocatedGraphicsMemoryIndex >= maxSuccessAllocatedGraphicsMemoryIndex) {
        return nullptr;

    } else {
        auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(allocationData, status);
        if (allocation) {
            allocationInDevicePoolCreated = true;
            if (localMemorySupported[allocation->getRootDeviceIndex()]) {
                static_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::LocalMemory);
            }
        }
        successAllocatedGraphicsMemoryIndex++;
        return allocation;
    }
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    if (failInAllocateWithSizeAndAlignment) {
        return nullptr;
    }
    allocationCreated = true;
    alignAllocationData = allocationData;
    return OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(allocationData);
}

GraphicsAllocation *MockMemoryManager::allocate32BitGraphicsMemory(uint32_t rootDeviceIndex, size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType) {
    bool allocateMemory = ptr == nullptr;
    AllocationData allocationData{};
    MockAllocationProperties properties(rootDeviceIndex, allocateMemory, size, allocationType);
    getAllocationData(allocationData, properties, ptr, createStorageInfoFromProperties(properties));
    bool useLocalMemory = !allocationData.flags.useSystemMemory && this->localMemorySupported[rootDeviceIndex];
    return allocate32BitGraphicsMemoryImpl(allocationData, useLocalMemory);
}

GraphicsAllocation *MockMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) {
    allocate32BitGraphicsMemoryImplCalled = true;
    if (failAllocate32Bit) {
        return nullptr;
    }
    return OsAgnosticMemoryManager::allocate32BitGraphicsMemoryImpl(allocationData, useLocalMemory);
}

GraphicsAllocation *MockMemoryManager::createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) {
    auto allocation = OsAgnosticMemoryManager::createGraphicsAllocationFromExistingStorage(properties, ptr, multiGraphicsAllocation);
    createGraphicsAllocationFromExistingStorageCalled++;
    allocationsFromExistingStorage.push_back(allocation);
    return allocation;
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {
    this->failedAllocationsCount = failedAllocationsCount;
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment, bool enableLocalMemory)
    : MockMemoryManager(enableLocalMemory, executionEnvironment) {
    this->failedAllocationsCount = failedAllocationsCount;
}

} // namespace NEO
