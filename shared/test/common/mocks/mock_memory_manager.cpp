/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"

#include <cstring>

namespace NEO {

const unsigned int MockMemoryManager::moduleId = 123u;
const unsigned int MockMemoryManager::serverType = 456u;

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
    validateAllocateProperties(properties);
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
    validateAllocateProperties(properties);
    lastAllocationProperties.reset(new AllocationProperties(properties));
    if (returnFakeAllocation) {
        auto *allocation{new GraphicsAllocation(properties.rootDeviceIndex, 1u /*num gmms*/, properties.allocationType, const_cast<void *>(ptr), dummyAddress, properties.size, 0, MemoryPool::system4KBPages, maxOsContextCount)};

        AllocationData allocationData;
        getAllocationData(allocationData, properties, const_cast<void *>(ptr), createStorageInfoFromProperties(properties));
        allocation->storageInfo = allocationData.storageInfo;
        return allocation;
    }
    if (isMockHostMemoryManager) {
        allocateGraphicsMemoryWithPropertiesCount++;
        if (forceFailureInAllocationWithHostPointer) {
            if (singleFailureInAllocationWithHostPointer) {
                forceFailureInAllocationWithHostPointer = false;
            }

            return nullptr;
        }

        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties, ptr);
    }

    recentlyPassedDeviceBitfield = properties.subDevicesBitfield;
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

GraphicsAllocation *MockMemoryManager::allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {
    return OsAgnosticMemoryManager::allocatePhysicalDeviceMemory(allocationData, status);
}

GraphicsAllocation *MockMemoryManager::allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) {
    return OsAgnosticMemoryManager::allocatePhysicalLocalDeviceMemory(allocationData, status);
}

GraphicsAllocation *MockMemoryManager::allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) {
    return OsAgnosticMemoryManager::allocatePhysicalHostMemory(allocationData, status);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemory64kb(const AllocationData &allocationData) {
    allocation64kbPageCreated = true;
    preferCompressedFlagPassed = forceCompressed ? true : allocationData.flags.preferCompressed;

    auto allocation = OsAgnosticMemoryManager::allocateGraphicsMemory64kb(allocationData);
    if (allocation) {
        allocation->getDefaultGmm()->setCompressionEnabled(preferCompressedFlagPassed);
    }
    return allocation;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    if (returnMockGAFromDevicePool) {
        status = AllocationStatus::Success;
        return mockGa;
    }
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
                static_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::localMemory);
            }
        }
        successAllocatedGraphicsMemoryIndex++;
        return allocation;
    }
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) {
    if (returnMockGAFromHostPool) {
        return mockGa;
    }
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
    return allocate32BitGraphicsMemoryImpl(allocationData);
}

GraphicsAllocation *MockMemoryManager::allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) {
    allocate32BitGraphicsMemoryImplCalled = true;
    if (failAllocate32Bit) {
        return nullptr;
    }
    return OsAgnosticMemoryManager::allocate32BitGraphicsMemoryImpl(allocationData);
}

void MockMemoryManager::forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range) {
    getGfxPartition(rootDeviceIndex)->init(range, 0, 0, gfxPartitions.size(), false, 0u, range + 1);
}

bool MockMemoryManager::hasPageFaultsEnabled(const Device &neoDevice) {
    if (debugManager.flags.EnableRecoverablePageFaults.get() != -1) {
        return !!debugManager.flags.EnableRecoverablePageFaults.get();
    }
    return false;
}

bool MockMemoryManager::isKmdMigrationAvailable(uint32_t rootDeviceIndex) {
    if (debugManager.flags.UseKmdMigration.get() != -1) {
        return !!debugManager.flags.UseKmdMigration.get();
    }
    return false;
}

GraphicsAllocation *MockMemoryManager::createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) {
    auto allocation = OsAgnosticMemoryManager::createGraphicsAllocationFromExistingStorage(properties, ptr, multiGraphicsAllocation);
    createGraphicsAllocationFromExistingStorageCalled++;
    allocationsFromExistingStorage.push_back(allocation);
    return allocation;
}

GraphicsAllocation *MockMemoryManager::createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) {
    if (osHandleData.handle != invalidSharedHandle) {
        auto allocation = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandleData, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
        this->capturedSharedHandle = osHandleData.handle;
        return allocation;
    } else {
        this->capturedSharedHandle = osHandleData.handle;
        return nullptr;
    }
}

bool MockMemoryManager::copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) {
    copyMemoryToAllocationBanksCalled++;
    copyMemoryToAllocationBanksParamsPassed.push_back({graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, handleMask});
    return OsAgnosticMemoryManager::copyMemoryToAllocationBanks(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, handleMask);
};

bool MockMemoryManager::reInitDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) {
    if (gfxPartitions.at(rootDeviceIndex) == nullptr) {
        // 4 x sizeof(Heap32) + 2 x sizeof(Standard/Standard64k)
        size_t reservedCpuAddressRangeSize = static_cast<size_t>((4 * 4 + 2 * 4)) * static_cast<size_t>(MemoryConstants::gigaByte);
        gfxPartitions.at(rootDeviceIndex) = std::make_unique<GfxPartition>(reservedCpuAddressRange);

        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;
        auto gfxTop = gpuAddressSpace + 1;
        if (getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, reservedCpuAddressRangeSize, rootDeviceIndex, gfxPartitions.size(), heapAssigners[rootDeviceIndex]->apiAllowExternalHeapForSshAndDsh, OsAgnosticMemoryManager::getSystemSharedMemory(rootDeviceIndex), gfxTop)) {
            return true;
        }
    }
    return false;
}

void MockMemoryManager::releaseDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) {
    gfxPartitions.at(rootDeviceIndex).reset();
}

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
    static_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::systemCpuInaccessible);
    return allocation;
}

OsContext *MockMemoryManagerOsAgnosticContext::createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                                                          const EngineDescriptor &engineDescriptor) {
    auto osContext = new OsContext(commandStreamReceiver->getRootDeviceIndex(), 0, engineDescriptor);
    osContext->incRefInternal();
    allRegisteredEngines[commandStreamReceiver->getRootDeviceIndex()].emplace_back(commandStreamReceiver, osContext);
    return osContext;
}

OsContext *MockMemoryManagerOsAgnosticContext::createAndRegisterSecondaryOsContext(const OsContext *primaryContext, CommandStreamReceiver *commandStreamReceiver,
                                                                                   const EngineDescriptor &engineDescriptor) {
    auto rootDeviceIndex = commandStreamReceiver->getRootDeviceIndex();

    auto osContext = new OsContext(rootDeviceIndex, 0, engineDescriptor);
    osContext->incRefInternal();

    osContext->setPrimaryContext(primaryContext);

    UNRECOVERABLE_IF(rootDeviceIndex != osContext->getRootDeviceIndex());

    secondaryEngines[rootDeviceIndex].emplace_back(commandStreamReceiver, osContext);
    allRegisteredEngines[rootDeviceIndex].emplace_back(commandStreamReceiver, osContext);

    return osContext;
}

OsContext *MockMemoryManagerWithDebuggableOsContext::createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                                                                const EngineDescriptor &engineDescriptor) {
    auto osContext = new MockOsContext(0, engineDescriptor);
    osContext->debuggableContext = true;
    osContext->incRefInternal();
    allRegisteredEngines[commandStreamReceiver->getRootDeviceIndex()].emplace_back(commandStreamReceiver, osContext);
    return osContext;
}

} // namespace NEO
