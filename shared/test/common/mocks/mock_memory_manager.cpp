/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"

#include <cstring>

namespace NEO {

MockMemoryManager::MockMemoryManager(bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, enableLocalMemory, executionEnvironment) {
    hostPtrManager.reset(new MockHostPtrManager);
}

MockMemoryManager::MockMemoryManager() : MockMemoryManager(*(new MockExecutionEnvironment(defaultHwInfo.get()))) {
    mockExecutionEnvironment.reset(static_cast<MockExecutionEnvironment *>(&executionEnvironment));
    mockExecutionEnvironment->initGmm();
}

MockMemoryManager::MockMemoryManager(bool enable64pages, bool enableLocalMemory) : MemoryManagerCreate(enable64pages, enableLocalMemory, *(new MockExecutionEnvironment(defaultHwInfo.get()))) {
    mockExecutionEnvironment.reset(static_cast<MockExecutionEnvironment *>(&executionEnvironment));
}

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

    if (fakeBigAllocations && size > bigAllocation) {
        size = MemoryConstants::pageSize64k;
    }

    return OsAgnosticMemoryManager::allocateSystemMemory(redundancyRatio * size, alignment);
}

void MockMemoryManager::waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation) {
    waitForEnginesCompletionCalled++;
    if (waitAllocations.get()) {
        waitAllocations->addAllocation(&graphicsAllocation);
    }
    MemoryManager::waitForEnginesCompletion(graphicsAllocation);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) {
    if (isMockHostMemoryManager) {
        allocateGraphicsMemoryWithPropertiesCount++;
        if (forceFailureInPrimaryAllocation) {
            if (singleFailureInPrimaryAllocation) {
                forceFailureInPrimaryAllocation = false;
            }
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }

    recentlyPassedDeviceBitfield = properties.subDevicesBitfield;
    AllocationProperties adjustedProperties(properties);
    adjustedProperties.size = redundancyRatio * properties.size;
    adjustedProperties.rootDeviceIndex = properties.rootDeviceIndex;
    return OsAgnosticMemoryManager::allocateGraphicsMemoryWithProperties(adjustedProperties);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) {
    lastAllocationProperties.reset(new AllocationProperties(properties));
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

GraphicsAllocation *MockMemoryManager::allocateMemoryByKMD(const AllocationData &allocationData) {
    allocateForShareableCalled = true;
    return OsAgnosticMemoryManager::allocateMemoryByKMD(allocationData);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    allocation64kbPageCreated = true;
    preferCompressedFlagPassed = forceCompressed ? true : allocationData.flags.preferCompressed;

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemory64kb(allocationData);
    if (allocation) {
        allocation->getDefaultGmm()->isCompressionEnabled = preferCompressedFlagPassed;
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

GraphicsAllocation *MockMemoryManager::allocate32BitGraphicsMemory(uint32_t rootDeviceIndex, size_t size, const void *ptr, AllocationType allocationType) {
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

void MockMemoryManager::forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range) {
    getGfxPartition(rootDeviceIndex)->init(range, 0, 0, gfxPartitions.size());
}

bool MockMemoryManager::isKmdMigrationAvailable(uint32_t rootDeviceIndex) {
    if (DebugManager.flags.UseKmdMigration.get() != -1) {
        return !!DebugManager.flags.UseKmdMigration.get();
    }
    return false;
}

GraphicsAllocation *MockMemoryManager::createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) {
    auto allocation = OsAgnosticMemoryManager::createGraphicsAllocationFromExistingStorage(properties, ptr, multiGraphicsAllocation);
    createGraphicsAllocationFromExistingStorageCalled++;
    allocationsFromExistingStorage.push_back(allocation);
    return allocation;
}

GraphicsAllocation *MockMemoryManager::createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) {
    if (handle != invalidSharedHandle) {
        auto allocation = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation);
        this->capturedSharedHandle = handle;
        return allocation;
    } else {
        this->capturedSharedHandle = handle;
        return nullptr;
    }
}

GraphicsAllocation *MockMemoryManager::createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) {
    if (toOsHandle(handle) != invalidSharedHandle) {
        auto graphicsAllocation = createMemoryAllocation(NEO::AllocationType::SHARED_BUFFER, nullptr, reinterpret_cast<void *>(1), 1,
                                                         4096u, toOsHandle(handle), MemoryPool::SystemCpuInaccessible, rootDeviceIndex,
                                                         false, false, false);
        graphicsAllocation->setSharedHandle(toOsHandle(handle));
        this->capturedSharedHandle = toOsHandle(handle);
        return graphicsAllocation;
    } else {
        this->capturedSharedHandle = toOsHandle(handle);
        return nullptr;
    }
}

bool MockMemoryManager::copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) {
    copyMemoryToAllocationBanksCalled++;
    copyMemoryToAllocationBanksParamsPassed.push_back({graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, handleMask});
    return OsAgnosticMemoryManager::copyMemoryToAllocationBanks(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, handleMask);
};

void *MockAllocSysMemAgnosticMemoryManager::allocateSystemMemory(size_t size, size_t alignment) {
    constexpr size_t minAlignment = 16;
    alignment = std::max(alignment, minAlignment);
    return alignedMalloc(size, alignment);
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {
    this->failedAllocationsCount = failedAllocationsCount;
}

FailMemoryManager::FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment, bool enableLocalMemory)
    : MockMemoryManager(enableLocalMemory, executionEnvironment) {
    this->failedAllocationsCount = failedAllocationsCount;
}

GraphicsAllocation *MockMemoryManagerFailFirstAllocation::allocateNonSystemGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    auto allocation = baseAllocateGraphicsMemoryInDevicePool(allocationData, status);
    if (!allocation) {
        allocation = allocateGraphicsMemory(allocationData);
    }
    static_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);
    return allocation;
}

OsContext *MockMemoryManagerOsAgnosticContext::createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                                                          const EngineDescriptor &engineDescriptor) {
    auto osContext = new OsContext(commandStreamReceiver->getRootDeviceIndex(), 0, engineDescriptor);
    osContext->incRefInternal();
    registeredEngines.emplace_back(commandStreamReceiver, osContext);
    return osContext;
}

OsContext *MockMemoryManagerWithDebuggableOsContext::createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                                                                const EngineDescriptor &engineDescriptor) {
    auto osContext = new MockOsContext(0, engineDescriptor);
    osContext->debuggableContext = true;
    osContext->incRefInternal();
    registeredEngines.emplace_back(commandStreamReceiver, osContext);
    return osContext;
}

} // namespace NEO
