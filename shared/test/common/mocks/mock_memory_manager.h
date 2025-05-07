/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {

template <class T>
class MemoryManagerCreate : public T {
  public:
    using T::T;

    template <class... U>
    MemoryManagerCreate(bool enable64kbPages, bool enableLocalMemory, U &&...args) : T(std::forward<U>(args)...) {
        std::fill(this->enable64kbpages.begin(), this->enable64kbpages.end(), enable64kbPages);
        std::fill(this->localMemorySupported.begin(), this->localMemorySupported.end(), enableLocalMemory);
    }
};

class MockMemoryManager : public MemoryManagerCreate<OsAgnosticMemoryManager> {
  public:
    using MemoryManager::allocateGraphicsMemoryInPreferredPool;
    using MemoryManager::allocateGraphicsMemoryWithAlignment;
    using MemoryManager::allocateGraphicsMemoryWithProperties;
    using MemoryManager::allRegisteredEngines;
    using MemoryManager::createGraphicsAllocation;
    using MemoryManager::createStorageInfoFromProperties;
    using MemoryManager::defaultEngineIndex;
    using MemoryManager::externalLocalMemoryUsageBankSelector;
    using MemoryManager::getAllocationData;
    using MemoryManager::getLocalOnlyRequired;
    using MemoryManager::gfxPartitions;
    using MemoryManager::internalLocalMemoryUsageBankSelector;
    using MemoryManager::isNonSvmBuffer;
    using MemoryManager::multiContextResourceDestructor;
    using MemoryManager::overrideAllocationData;
    using MemoryManager::pageFaultManager;
    using MemoryManager::prefetchManager;
    using MemoryManager::singleTemporaryAllocationsList;
    using MemoryManager::supportsMultiStorageResources;
    using MemoryManager::temporaryAllocations;
    using MemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory;
    using MemoryManager::unMapPhysicalHostMemoryFromVirtualMemory;
    using MemoryManager::useNonSvmHostPtrAlloc;
    using MemoryManager::usmReuseInfo;
    using OsAgnosticMemoryManager::allocateGraphicsMemoryForImageFromHostPtr;
    using MemoryManagerCreate<OsAgnosticMemoryManager>::MemoryManagerCreate;
    using MemoryManager::customHeapAllocators;
    using MemoryManager::enable64kbpages;
    using MemoryManager::executionEnvironment;
    using MemoryManager::getPreferredAllocationMethod;
    using MemoryManager::isaInLocalMemory;
    using MemoryManager::isAllocationTypeToCapture;
    using MemoryManager::isCopyRequired;
    using MemoryManager::latestContextId;
    using MemoryManager::localMemAllocsSize;
    using MemoryManager::localMemorySupported;
    using MemoryManager::reservedMemory;
    using MemoryManager::secondaryEngines;

    static constexpr osHandle invalidSharedHandle = -1;
    static const unsigned int moduleId;
    static const unsigned int serverType;

    MockMemoryManager(ExecutionEnvironment &executionEnvironment) : MockMemoryManager(false, executionEnvironment) {}

    MockMemoryManager(bool enableLocalMemory, ExecutionEnvironment &executionEnvironment);
    MockMemoryManager();
    MockMemoryManager(bool enable64pages, bool enableLocalMemory);

    ADDMETHOD_NOBASE(registerSysMemAlloc, AllocationStatus, AllocationStatus::Success, (GraphicsAllocation * allocation));
    ADDMETHOD_NOBASE(registerLocalMemAlloc, AllocationStatus, AllocationStatus::Success, (GraphicsAllocation * allocation, uint32_t rootDeviceIndex));

    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    void setDeferredDeleter(DeferredDeleter *deleter);
    void overrideAsyncDeleterFlag(bool newValue);
    GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override;
    GraphicsAllocation *allocatePhysicalLocalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocatePhysicalDeviceMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocatePhysicalHostMemory(const AllocationData &allocationData, AllocationStatus &status) override;
    int redundancyRatio = 1;

    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override;
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) override;
    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override;

    void *allocateSystemMemory(size_t size, size_t alignment) override;

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
        freeGraphicsMemoryCalled++;
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(gfxAllocation);
    };

    AddressRange reserveGpuAddressOnHeap(const uint64_t requiredStartAddress, size_t size, const RootDeviceIndicesContainer &rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex, HeapIndex heap, size_t alignment) override {
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

    struct ReserveGpuAddressOnHeapParams {
        uint64_t requiredStartAddress{};
        size_t size{};
        RootDeviceIndicesContainer rootDeviceIndices{};
        uint32_t *reservedOnRootDeviceIndex{};
        HeapIndex heap{};
        size_t alignment{};
    };

    StackVec<ReserveGpuAddressOnHeapParams, 2> reserveGpuAddressOnHeapParamsPassed{};
    StackVec<size_t, 5> reserveGpuAddressOnHeapFailOnCalls;

    void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) override {
        freeGpuAddressCalled++;
        return OsAgnosticMemoryManager::freeGpuAddress(addressRange, rootDeviceIndex);
    }

    void *lockResourceImpl(GraphicsAllocation &gfxAllocation) override {
        lockResourceCalled++;
        void *pLockedMemory = nullptr;
        if (!failLockResource) {
            pLockedMemory = OsAgnosticMemoryManager::lockResourceImpl(gfxAllocation);
        }
        lockResourcePointers.push_back(pLockedMemory);
        return pLockedMemory;
    }

    void unlockResourceImpl(GraphicsAllocation &gfxAllocation) override {
        unlockResourceCalled++;
        OsAgnosticMemoryManager::unlockResourceImpl(gfxAllocation);
    }

    bool allocInUse(GraphicsAllocation &graphicsAllocation) const override {
        allocInUseCalled++;

        if (callBaseAllocInUse) {
            return OsAgnosticMemoryManager::allocInUse(graphicsAllocation);
        }

        if (deferAllocInUse) {
            return true;
        }
        return false;
    }

    void waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation) override;

    void handleFenceCompletion(GraphicsAllocation *graphicsAllocation) override {
        handleFenceCompletionCalled++;
        OsAgnosticMemoryManager::handleFenceCompletion(graphicsAllocation);
    }

    void *reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) override {
        if (failReserveAddress) {
            return nullptr;
        }
        return OsAgnosticMemoryManager::reserveCpuAddressRange(size, rootDeviceIndex);
    }

    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override {
        if (failReserveAddress) {
            return {};
        }
        return OsAgnosticMemoryManager::reserveCpuAddress(requiredStartAddress, size);
    }

    void freeCpuAddress(AddressRange addressRange) override {
        return OsAgnosticMemoryManager::freeCpuAddress(addressRange);
    }

    void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices,
                                                          AllocationProperties &properties,
                                                          MultiGraphicsAllocation &multiGraphicsAllocation) override {
        if (isMockEventPoolCreateMemoryManager) {
            return nullptr;
        }
        return OsAgnosticMemoryManager::createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphicsAllocation);
    }

    bool isCpuCopyRequired(const void *ptr) override { return cpuCopyRequired; }

    GraphicsAllocation *allocate32BitGraphicsMemory(uint32_t rootDeviceIndex, size_t size, const void *ptr, AllocationType allocationType);
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override;

    bool isLimitedGPU(uint32_t rootDeviceIndex) override {
        return limitedGPU;
    }
    void forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range);

    bool setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) override {
        memAdviseFlags = flags;
        if (failSetMemAdvise) {
            return false;
        }
        return MemoryManager::setMemAdvise(gfxAllocation, flags, rootDeviceIndex);
    }

    bool setSharedSystemMemAdvise(const void *ptr, const size_t size, MemAdvise memAdviseOp, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override {
        setSharedSystemMemAdviseCalledCount++;
        setSharedSystemMemAdviseCalled = true;
        if (failSetSharedSystemMemAdvise) {
            return false;
        }
        return MemoryManager::setSharedSystemMemAdvise(ptr, size, memAdviseOp, subDeviceIds, rootDeviceIndex);
    }

    bool setMemPrefetch(GraphicsAllocation *gfxAllocation, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override {
        memPrefetchSubDeviceIds = subDeviceIds;
        setMemPrefetchCalled = true;
        setMemPrefetchCalledCount++;
        return MemoryManager::setMemPrefetch(gfxAllocation, subDeviceIds, rootDeviceIndex);
    }

    bool setSharedSystemAtomicAccess(const void *ptr, const size_t size, AtomicAccessMode mode, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override {
        setSharedSystemAtomicAccessCalledCount++;
        setSharedSystemAtomicAccessCalled = true;
        if (failSetSharedSystemAtomicAccess) {
            return false;
        }
        return MemoryManager::setSharedSystemAtomicAccess(ptr, size, mode, subDeviceIds, rootDeviceIndex);
    }

    bool prefetchSharedSystemAlloc(const void *ptr, const size_t size, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) override {
        memPrefetchSubDeviceIds = subDeviceIds;
        prefetchSharedSystemAllocCalled = true;
        return MemoryManager::prefetchSharedSystemAlloc(ptr, size, subDeviceIds, rootDeviceIndex);
    }

    bool hasPageFaultsEnabled(const Device &neoDevice) override;
    bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) override;

    struct CopyMemoryToAllocationBanksParams {
        GraphicsAllocation *graphicsAllocation = nullptr;
        size_t destinationOffset = 0u;
        const void *memoryToCopy = nullptr;
        size_t sizeToCopy = 0u;
        DeviceBitfield handleMask = {};
    };

    StackVec<CopyMemoryToAllocationBanksParams, 2> copyMemoryToAllocationBanksParamsPassed{};
    bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) override;

    MemoryManager::AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override {
        populateOsHandlesCalled++;
        populateOsHandlesParamsPassed.push_back({handleStorage, rootDeviceIndex});
        if (callBasePopulateOsHandles) {
            populateOsHandlesResult = OsAgnosticMemoryManager::populateOsHandles(handleStorage, rootDeviceIndex);
        }
        return populateOsHandlesResult;
    }

    struct PopulateOsHandlesParams {
        OsHandleStorage handleStorage{};
        uint32_t rootDeviceIndex{};
    };

    StackVec<PopulateOsHandlesParams, 1> populateOsHandlesParamsPassed{};

    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override {
        allocateGraphicsMemoryForNonSvmHostPtrCalled++;
        if (callBaseAllocateGraphicsMemoryForNonSvmHostPtr) {
            allocateGraphicsMemoryForNonSvmHostPtrResult = OsAgnosticMemoryManager::allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
        }
        return allocateGraphicsMemoryForNonSvmHostPtrResult;
    }

    bool allowIndirectAllocationsAsPack(uint32_t rootDeviceIndex) override {
        if (overrideAllocateAsPackReturn != -1) {
            return !!overrideAllocateAsPackReturn;
        } else {
            return MemoryManager::allowIndirectAllocationsAsPack(rootDeviceIndex);
        }
    }

    bool mapPhysicalDeviceMemoryToVirtualMemory(GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override {
        if (failMapPhysicalToVirtualMemory) {
            return false;
        }
        return OsAgnosticMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory(physicalAllocation, gpuRange, bufferSize);
    }

    bool mapPhysicalHostMemoryToVirtualMemory(RootDeviceIndicesContainer &rootDeviceIndices, MultiGraphicsAllocation &multiGraphicsAllocation, GraphicsAllocation *physicalAllocation, uint64_t gpuRange, size_t bufferSize) override {
        if (failMapPhysicalToVirtualMemory) {
            return false;
        }
        return OsAgnosticMemoryManager::mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation, gpuRange, bufferSize);
    }

    void registerIpcExportedAllocation(GraphicsAllocation *graphicsAllocation) override {
        registerIpcExportedAllocationCalled++;
    }

    void getExtraDeviceProperties(uint32_t rootDeviceIndex, uint32_t *moduleId, uint16_t *serverType) override {
        *moduleId = MockMemoryManager::moduleId;
        *serverType = MockMemoryManager::serverType;
    }

    bool reInitDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) override;
    void releaseDeviceSpecificGfxPartition(uint32_t rootDeviceIndex) override;

    MockGraphicsAllocation *mockGa;
    size_t ipcAllocationSize = 4096u;
    uint32_t copyMemoryToAllocationBanksCalled = 0u;
    uint32_t populateOsHandlesCalled = 0u;
    uint32_t allocateGraphicsMemoryForNonSvmHostPtrCalled = 0u;
    uint32_t freeGraphicsMemoryCalled = 0u;
    uint32_t reserveGpuAddressOnHeapCalled = 0u;
    uint32_t freeGpuAddressCalled = 0u;
    uint32_t unlockResourceCalled = 0u;
    uint32_t lockResourceCalled = 0u;
    uint32_t createGraphicsAllocationFromExistingStorageCalled = 0u;
    mutable uint32_t allocInUseCalled = 0u;
    uint32_t registerIpcExportedAllocationCalled = 0;
    int32_t overrideAllocateAsPackReturn = -1;
    std::vector<GraphicsAllocation *> allocationsFromExistingStorage{};
    AllocationData alignAllocationData;
    uint32_t successAllocatedGraphicsMemoryIndex = 0u;
    uint32_t maxSuccessAllocatedGraphicsMemoryIndex = std::numeric_limits<uint32_t>::max();
    std::vector<void *> lockResourcePointers;
    uint32_t handleFenceCompletionCalled = 0u;
    uint32_t waitForEnginesCompletionCalled = 0u;
    uint32_t allocateGraphicsMemoryWithPropertiesCount = 0;
    uint32_t setMemPrefetchCalledCount = 0;
    uint32_t setSharedSystemMemAdviseCalledCount = 0;
    uint32_t setSharedSystemAtomicAccessCalledCount = 0;
    osHandle capturedSharedHandle = 0u;
    bool allocationCreated = false;
    bool allocation64kbPageCreated = false;
    bool allocationInDevicePoolCreated = false;
    bool failInDevicePool = false;
    bool failInDevicePoolWithError = false;
    bool failInAllocateWithSizeAndAlignment = false;
    bool preferCompressedFlagPassed = false;
    bool allocateForImageCalled = false;
    bool allocate32BitGraphicsMemoryImplCalled = false;
    bool allocateForShareableCalled = false;
    bool failReserveAddress = false;
    bool failReserveGpuAddressOnHeap = false;
    bool failAllocateSystemMemory = false;
    bool failAllocate32Bit = false;
    bool failLockResource = false;
    bool failSetMemAdvise = false;
    bool failSetSharedSystemMemAdvise = false;
    bool failSetSharedSystemAtomicAccess = false;
    bool setSharedSystemMemAdviseCalled = false;
    bool setMemPrefetchCalled = false;
    bool setSharedSystemAtomicAccessCalled = false;
    bool prefetchSharedSystemAllocCalled = false;
    bool cpuCopyRequired = false;
    bool forceCompressed = false;
    bool forceFailureInPrimaryAllocation = false;
    bool singleFailureInPrimaryAllocation = false;
    bool forceFailureInAllocationWithHostPointer = false;
    bool singleFailureInAllocationWithHostPointer = false;
    bool isMockHostMemoryManager = false;
    bool deferAllocInUse = false;
    bool callBaseAllocInUse = false;
    bool isMockEventPoolCreateMemoryManager = false;
    bool limitedGPU = false;
    bool returnFakeAllocation = false;
    bool callBasePopulateOsHandles = true;
    bool callBaseAllocateGraphicsMemoryForNonSvmHostPtr = true;
    bool failMapPhysicalToVirtualMemory = false;
    bool returnMockGAFromDevicePool = false;
    bool returnMockGAFromHostPool = false;
    std::unique_ptr<MockExecutionEnvironment> mockExecutionEnvironment;
    DeviceBitfield recentlyPassedDeviceBitfield{};
    std::unique_ptr<MultiGraphicsAllocation> waitAllocations = nullptr;
    SubDeviceIdsVec memPrefetchSubDeviceIds;
    MemAdviseFlags memAdviseFlags{};
    MemoryManager::AllocationStatus populateOsHandlesResult = MemoryManager::AllocationStatus::Success;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtrResult = nullptr;
    std::unique_ptr<AllocationProperties> lastAllocationProperties = nullptr;
    std::function<void(const AllocationProperties &)> validateAllocateProperties = [](const AllocationProperties &) -> void {};
    AddressRange reserveGpuAddressOnHeapResult = AddressRange{0u, 0u};
};

class MockAllocSysMemAgnosticMemoryManager : public OsAgnosticMemoryManager {
  public:
    MockAllocSysMemAgnosticMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {
        ptrRestrictions = nullptr;
        testRestrictions.minAddress = 0;
    }

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override {
        return ptrRestrictions;
    }

    void *allocateSystemMemory(size_t size, size_t alignment) override;

    AlignedMallocRestrictions testRestrictions;
    AlignedMallocRestrictions *ptrRestrictions;
};

class FailMemoryManager : public MockMemoryManager {
  public:
    using MemoryManager::allocateGraphicsMemoryWithProperties;
    using MockMemoryManager::MockMemoryManager;
    FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment);
    FailMemoryManager(int32_t failedAllocationsCount, ExecutionEnvironment &executionEnvironment, bool localMemory);

    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override {
        if (failedAllocationsCount <= 0) {
            return nullptr;
        }
        failedAllocationsCount--;
        return OsAgnosticMemoryManager::allocateGraphicsMemoryWithAlignment(allocationData);
    }
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) override {
        return nullptr;
    }
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override {
        return nullptr;
    }
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) override {
        return nullptr;
    }
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) override {
        return nullptr;
    }

    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        return nullptr;
    }

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        return nullptr;
    }

    void *lockResourceImpl(GraphicsAllocation &gfxAllocation) override { return nullptr; };
    void unlockResourceImpl(GraphicsAllocation &gfxAllocation) override{};

    MemoryManager::AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override {
        return AllocationStatus::Error;
    };
    void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) override{};

    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override {
        return 0;
    };

    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override {
        return nullptr;
    };
    GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override {
        return nullptr;
    }
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override {
        return nullptr;
    }
    int32_t failedAllocationsCount = 0;
};

class MockMemoryManagerFailFirstAllocation : public MockMemoryManager {
  public:
    MockMemoryManagerFailFirstAllocation(bool enableLocalMemory, const ExecutionEnvironment &executionEnvironment) : MockMemoryManager(enableLocalMemory, const_cast<ExecutionEnvironment &>(executionEnvironment)){};
    MockMemoryManagerFailFirstAllocation(const ExecutionEnvironment &executionEnvironment) : MockMemoryManagerFailFirstAllocation(false, executionEnvironment){};

    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override {
        allocateGraphicsMemoryInDevicePoolCalled++;
        if (returnNullptr) {
            returnNullptr = false;
            return nullptr;
        }
        if (returnAllocateNonSystemGraphicsMemoryInDevicePool) {
            returnAllocateNonSystemGraphicsMemoryInDevicePool = false;
            return allocateNonSystemGraphicsMemoryInDevicePool(allocationData, status);
        }
        if (returnBaseAllocateGraphicsMemoryInDevicePool) {
            return baseAllocateGraphicsMemoryInDevicePool(allocationData, status);
        }
        return allocateGraphicsMemoryInDevicePoolResult;
    }

    uint32_t allocateGraphicsMemoryInDevicePoolCalled = 0u;
    GraphicsAllocation *allocateGraphicsMemoryInDevicePoolResult = nullptr;
    bool returnNullptr = false;
    bool returnBaseAllocateGraphicsMemoryInDevicePool = false;
    bool returnAllocateNonSystemGraphicsMemoryInDevicePool = false;

    GraphicsAllocation *baseAllocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
        return OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(allocationData, status);
    }

    GraphicsAllocation *allocateNonSystemGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status);
};

class MockMemoryManagerOsAgnosticContext : public MockMemoryManager {
  public:
    MockMemoryManagerOsAgnosticContext(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
    OsContext *createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                          const EngineDescriptor &engineDescriptor) override;
    OsContext *createAndRegisterSecondaryOsContext(const OsContext *primaryContext, CommandStreamReceiver *commandStreamReceiver,
                                                   const EngineDescriptor &engineDescriptor) override;
};

class MockMemoryManagerWithDebuggableOsContext : public MockMemoryManager {
  public:
    MockMemoryManagerWithDebuggableOsContext(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
    OsContext *createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                          const EngineDescriptor &engineDescriptor) override;
};

class MockMemoryManagerWithCapacity : public MockMemoryManager {
  public:
    MockMemoryManagerWithCapacity(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) override {
        if (this->capacity >= properties.size) {
            this->capacity -= properties.size;
            return MockMemoryManager::allocateGraphicsMemoryWithProperties(properties, ptr);
        }
        return nullptr;
    }

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        return this->allocateGraphicsMemoryWithProperties(properties, nullptr);
    }

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
        this->capacity += gfxAllocation->getUnderlyingBufferSize();
        MockMemoryManager::freeGraphicsMemoryImpl(gfxAllocation);
    };

    size_t capacity = 0u;
};

class MemoryManagerMemHandleMock : public MockMemoryManager {
  public:
    bool isNTHandle(osHandle handle, uint32_t rootDeviceIndex) override {
        return ntHandle;
    };

    bool ntHandle = false;
};

} // namespace NEO
