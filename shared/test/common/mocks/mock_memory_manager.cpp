/*
 * Copyright (C) 2018-2026 Intel Corporation
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
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
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
    if (waitForEnginesCompletionCallback) {
        waitForEnginesCompletionCallback();
    }
    if (waitAllocations.get()) {
        waitAllocations->addAllocation(&graphicsAllocation);
    }
    MemoryManager::waitForEnginesCompletion(graphicsAllocation);
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) {
    validateAllocateProperties(properties);
    allocateGraphicsMemoryWithPropertiesCalledCount++;
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

    {
        std::lock_guard<std::mutex> lock(mockStateMutex);
        recentlyPassedDeviceBitfield = properties.subDevicesBitfield;
    }
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

    {
        std::lock_guard<std::mutex> lock(mockStateMutex);
        recentlyPassedDeviceBitfield = properties.subDevicesBitfield;
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
            if (unifiedSharedMemoryUseGpuVaAsCpuPtr && allocationData.type == AllocationType::unifiedSharedMemory) {
                allocation->setCpuPtrAndGpuAddress(reinterpret_cast<void *>(allocation->getGpuAddress()), allocation->getGpuAddress());
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
    {
        std::lock_guard<std::mutex> lock(mockStateMutex);
        allocationCreated = true;
        alignAllocationData = allocationData;
    }
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
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    getGfxPartition(rootDeviceIndex)->init(range, 0, 0, gfxPartitions.size(), false, 0u, range + 1, &productHelper);
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
    this->capturedPhysicalOffset = osHandleData.physicalOffset;
    createGraphicsAllocationFromSharedHandleCalled++;
    if (properties.rootDeviceIndex == createGraphicsAllocationFromSharedHandleFailingRootDeviceIndex) {
        this->capturedSharedHandle = osHandleData.handle;
        return nullptr;
    }
    if (osHandleData.handle != invalidSharedHandle) {
        if (storeIpcAllocations && mapPointer == nullptr) {
            auto alloc = storedIpcAllocations.find(osHandleData.handle);
            if (alloc != storedIpcAllocations.end()) {
                mapPointer = alloc->second->getUnderlyingBuffer();
            }
        }

        auto allocation = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandleData, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
        this->capturedSharedHandle = osHandleData.handle;
        this->capturedIsHostIpcAllocation.push_back(isHostIpcAllocation);
        if (this->setGmmInSharedHandleAllocation) {
            auto gmm = std::make_unique<MockGmm>(this->getGmmHelper(properties.rootDeviceIndex));
            this->sharedHandleAllocationGmm = gmm.get();
            allocation->setDefaultGmm(std::move(gmm));
        }
        return allocation;
    } else {
        this->capturedSharedHandle = osHandleData.handle;
        return nullptr;
    }
}

bool MockMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    copyMemoryToAllocationCalled++;
    return OsAgnosticMemoryManager::copyMemoryToAllocation(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy);
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

        auto &productHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
        auto gpuAddressSpace = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.gpuAddressSpace;
        auto gfxTop = gpuAddressSpace + 1;
        if (getGfxPartition(rootDeviceIndex)->init(gpuAddressSpace, reservedCpuAddressRangeSize, rootDeviceIndex, gfxPartitions.size(), heapAssigners[rootDeviceIndex]->apiAllowExternalHeapForSshAndDsh, OsAgnosticMemoryManager::getSystemSharedMemory(rootDeviceIndex), gfxTop, &productHelper)) {
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

void MockMemoryManager::freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) {
    freeGraphicsMemoryCalled++;
    OsAgnosticMemoryManager::freeGraphicsMemoryImpl(gfxAllocation);
}

AddressRange MockMemoryManager::reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) {
    reserveGpuAddressOnHeapCalled++;
    reserveGpuAddressOnHeapParamsPassed.push_back({requiredStartAddress, size, rootDeviceIndices, reservedOnRootDeviceIndex, heap, alignment});

    bool shouldFail = failReserveGpuAddressOnHeap;
    if (!reserveGpuAddressOnHeapFailOnCalls.empty() &&
        std::find(reserveGpuAddressOnHeapFailOnCalls.begin(), reserveGpuAddressOnHeapFailOnCalls.end(), reserveGpuAddressOnHeapCalled - 1) != reserveGpuAddressOnHeapFailOnCalls.end()) {
        shouldFail = true;
    }

    if (shouldFail) {
        reserveGpuAddressOnHeapResult = AddressRange{0u, 0u};
    } else {
        reserveGpuAddressOnHeapResult = OsAgnosticMemoryManager::reserveGpuAddressOnHeap(requiredStartAddress, size, rootDeviceIndices, reservedOnRootDeviceIndex, heap, alignment);
    }
    return reserveGpuAddressOnHeapResult;
}

void MockMemoryManager::freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) {
    freeGpuAddressCalled++;
    return OsAgnosticMemoryManager::freeGpuAddress(addressRange, rootDeviceIndex);
}

void *MockMemoryManager::lockResourceImpl(GraphicsAllocation &gfxAllocation) {
    lockResourceCalled++;
    void *pLockedMemory = nullptr;
    if (!failLockResource) {
        pLockedMemory = OsAgnosticMemoryManager::lockResourceImpl(gfxAllocation);
    }
    lockResourcePointers.push_back(pLockedMemory);
    return pLockedMemory;
}

void MockMemoryManager::unlockResourceImpl(GraphicsAllocation &gfxAllocation) {
    unlockResourceCalled++;
    OsAgnosticMemoryManager::unlockResourceImpl(gfxAllocation);
}

bool MockMemoryManager::allocInUse(GraphicsAllocation &graphicsAllocation) {
    allocInUseCalled++;

    if (allocInUseCallback) {
        allocInUseCallback();
    }

    if (callBaseAllocInUse) {
        return OsAgnosticMemoryManager::allocInUse(graphicsAllocation);
    }

    if (deferAllocInUse) {
        return true;
    }
    return false;
}

void MockMemoryManager::handleFenceCompletion(GraphicsAllocation *graphicsAllocation) {
    handleFenceCompletionCalled++;
    OsAgnosticMemoryManager::handleFenceCompletion(graphicsAllocation);
}

void *MockMemoryManager::reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) {
    if (failReserveAddress) {
        return nullptr;
    }
    return OsAgnosticMemoryManager::reserveCpuAddressRange(size, rootDeviceIndex);
}

AddressRange MockMemoryManager::reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) {
    if (failReserveAddress) {
        return {};
    }
    return OsAgnosticMemoryManager::reserveCpuAddress(requiredStartAddress, size);
}

void *MockMemoryManager::createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices,
                                                                         AllocationProperties &properties,
                                                                         MultiGraphicsAllocation &multiGraphicsAllocation) {
    if (isMockEventPoolCreateMemoryManager) {
        return nullptr;
    }
    return OsAgnosticMemoryManager::createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphicsAllocation);
}

bool MockMemoryManager::setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) {
    memAdviseFlags = flags;
    if (failSetMemAdvise) {
        return false;
    }
    return MemoryManager::setMemAdvise(gfxAllocation, flags, rootDeviceIndex);
}

bool MockMemoryManager::setSharedSystemMemAdvise(const void *ptr, const size_t size, MemAdvise memAdviseOp, Device &callingDevice, Device &targetDevice) {
    setSharedSystemMemAdviseCalledCount++;
    setSharedSystemMemAdviseCalled = true;
    if (failSetSharedSystemMemAdvise) {
        return false;
    }
    return MemoryManager::setSharedSystemMemAdvise(ptr, size, memAdviseOp, callingDevice, targetDevice);
}

bool MockMemoryManager::setMemPrefetch(GraphicsAllocation *gfxAllocation, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {
    memPrefetchSubDeviceIds = subDeviceIds;
    setMemPrefetchCalled = true;
    setMemPrefetchCalledCount++;
    return MemoryManager::setMemPrefetch(gfxAllocation, subDeviceIds, rootDeviceIndex);
}

bool MockMemoryManager::setAtomicAccess(GraphicsAllocation *gfxAllocation, size_t size, AtomicAccessMode mode, uint32_t rootDeviceIndex) {

    if (failSetAtomicAccess) {
        return false;
    }
    return MemoryManager::setAtomicAccess(gfxAllocation, size, mode, rootDeviceIndex);
}

bool MockMemoryManager::setSharedSystemAtomicAccess(const void *ptr, const size_t size, AtomicAccessMode mode, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {
    setSharedSystemAtomicAccessCalledCount++;
    setSharedSystemAtomicAccessCalled = true;
    if (failSetSharedSystemAtomicAccess) {
        return false;
    }
    return MemoryManager::setSharedSystemAtomicAccess(ptr, size, mode, subDeviceIds, rootDeviceIndex);
}

AtomicAccessMode MockMemoryManager::getSharedSystemAtomicAccess(const void *ptr, const size_t size, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {
    getSharedSystemAtomicAccessCalledCount++;
    getSharedSystemAtomicAccessCalled = true;
    if (failGetSharedSystemAtomicAccess) {
        return AtomicAccessMode::invalid;
    }
    return MemoryManager::getSharedSystemAtomicAccess(ptr, size, subDeviceIds, rootDeviceIndex);
}

bool MockMemoryManager::prefetchSharedSystemAlloc(const void *ptr, const size_t size, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) {
    memPrefetchSubDeviceIds = subDeviceIds;
    prefetchSharedSystemAllocCalled = true;
    return MemoryManager::prefetchSharedSystemAlloc(ptr, size, subDeviceIds, rootDeviceIndex);
}

bool MockMemoryManager::memsetAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, int value, size_t sizeToSet) {
    memsetAllocationCalled++;
    return MemoryManager::memsetAllocation(graphicsAllocation, destinationOffset, value, sizeToSet);
}

MemoryManager::AllocationStatus MockMemoryManager::populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) {
    populateOsHandlesCalled++;
    populateOsHandlesParamsPassed.push_back({handleStorage, rootDeviceIndex});
    if (callBasePopulateOsHandles) {
        populateOsHandlesResult = OsAgnosticMemoryManager::populateOsHandles(handleStorage, rootDeviceIndex);
    }
    return populateOsHandlesResult;
}

GraphicsAllocation *MockMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) {
    allocateGraphicsMemoryForNonSvmHostPtrCalled++;
    if (callBaseAllocateGraphicsMemoryForNonSvmHostPtr) {
        allocateGraphicsMemoryForNonSvmHostPtrResult = OsAgnosticMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    }
    return allocateGraphicsMemoryForNonSvmHostPtrResult;
}

bool MockMemoryManager::allowIndirectAllocationsAsPack(uint32_t rootDeviceIndex) {
    if (overrideAllocateAsPackReturn != -1) {
        return !!overrideAllocateAsPackReturn;
    } else {
        return MemoryManager::allowIndirectAllocationsAsPack(rootDeviceIndex);
    }
}

bool MockMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, const MemoryFlags *memoryflags, size_t offset) {
    passedMapPhysicalDeviceMemoryOffset = offset;
    if (failMapPhysicalToVirtualMemory) {
        return false;
    }
    return OsAgnosticMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuRange, bufferSize, memoryflags, offset);
}

bool MockMemoryManager::mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, size_t offset, bool keepReservationPlaceholder) {
    passedMapPhysicalHostMemoryOffset = offset;
    if (failMapPhysicalToVirtualMemory) {
        return false;
    }
    return OsAgnosticMemoryManager::mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuRange, bufferSize, offset, keepReservationPlaceholder);
}

bool MockMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, OsContext *osContext, uint32_t rootDeviceIndex) {
    // Need to call actual unMapPhysicalDeviceMemoryFromVirtualMemory to ensure the virtual memory is unmapped.
    bool retVal = OsAgnosticMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuRange, bufferSize, osContext, rootDeviceIndex);
    if (failUnMapPhysicalToVirtualMemory) { // Return false if unmapping is supposed to fail.
        return false;
    }
    return retVal;
}

bool MockMemoryManager::unMapPhysicalHostMemoryFromVirtualMemory(MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize, bool keepReservationPlaceholder) {
    // Need to call actual unMapPhysicalDeviceMemoryFromVirtualMemory to ensure the virtual memory is unmapped.
    bool retVal = OsAgnosticMemoryManager::unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation, gpuRange, bufferSize, true);
    if (failUnMapPhysicalToVirtualMemory) { // Return false if unmapping is supposed to fail.
        return false;
    }
    return retVal;
}

std::unique_lock<std::mutex> MockMemoryManager::lockVirtualMemoryReservationMap() {
    lockVirtualMemoryReservationMapCalled++;
    return MemoryManager::lockVirtualMemoryReservationMap();
}

void MockMemoryManager::registerIpcExportedAllocation(GraphicsAllocation *graphicsAllocation) {
    registerIpcExportedAllocationCalled++;
    if (storeIpcAllocations) {
        storedIpcAllocations[static_cast<MemoryAllocation *>(graphicsAllocation)->internalHandle] = graphicsAllocation;
    }
}

void MockMemoryManager::getExtraDeviceProperties(uint32_t rootDeviceIndex, uint32_t *moduleId, uint16_t *serverType) {
    *moduleId = MockMemoryManager::moduleId;
    *serverType = MockMemoryManager::serverType;
}

} // namespace NEO
