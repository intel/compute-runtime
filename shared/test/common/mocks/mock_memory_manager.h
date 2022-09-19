/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
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
    using MemoryManager::createGraphicsAllocation;
    using MemoryManager::createStorageInfoFromProperties;
    using MemoryManager::defaultEngineIndex;
    using MemoryManager::externalLocalMemoryUsageBankSelector;
    using MemoryManager::getAllocationData;
    using MemoryManager::gfxPartitions;
    using MemoryManager::internalLocalMemoryUsageBankSelector;
    using MemoryManager::isNonSvmBuffer;
    using MemoryManager::multiContextResourceDestructor;
    using MemoryManager::overrideAllocationData;
    using MemoryManager::pageFaultManager;
    using MemoryManager::prefetchManager;
    using MemoryManager::registeredEngines;
    using MemoryManager::supportsMultiStorageResources;
    using MemoryManager::useNonSvmHostPtrAlloc;
    using OsAgnosticMemoryManager::allocateGraphicsMemoryForImageFromHostPtr;
    using MemoryManagerCreate<OsAgnosticMemoryManager>::MemoryManagerCreate;
    using MemoryManager::enable64kbpages;
    using MemoryManager::isaInLocalMemory;
    using MemoryManager::isAllocationTypeToCapture;
    using MemoryManager::isCopyRequired;
    using MemoryManager::localMemorySupported;
    using MemoryManager::reservedMemory;

    MockMemoryManager(ExecutionEnvironment &executionEnvironment) : MockMemoryManager(false, executionEnvironment) {}

    MockMemoryManager(bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, enableLocalMemory, executionEnvironment) {
        hostPtrManager.reset(new MockHostPtrManager);
    };

    MockMemoryManager() : MockMemoryManager(*(new MockExecutionEnvironment(defaultHwInfo.get()))) {
        mockExecutionEnvironment.reset(static_cast<MockExecutionEnvironment *>(&executionEnvironment));
        mockExecutionEnvironment->initGmm();
    };
    MockMemoryManager(bool enable64pages, bool enableLocalMemory) : MemoryManagerCreate(enable64pages, enableLocalMemory, *(new MockExecutionEnvironment(defaultHwInfo.get()))) {
        mockExecutionEnvironment.reset(static_cast<MockExecutionEnvironment *>(&executionEnvironment));
    }
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    void setDeferredDeleter(DeferredDeleter *deleter);
    void overrideAsyncDeleterFlag(bool newValue);
    GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) override;
    int redundancyRatio = 1;

    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override;
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) override;
    GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override;
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override;

    void *allocateSystemMemory(size_t size, size_t alignment) override;

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
        freeGraphicsMemoryCalled++;
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(gfxAllocation);
    };

    void *lockResourceImpl(GraphicsAllocation &gfxAllocation) override {
        lockResourceCalled++;
        auto pLockedMemory = OsAgnosticMemoryManager::lockResourceImpl(gfxAllocation);
        lockResourcePointers.push_back(pLockedMemory);
        return pLockedMemory;
    }

    void unlockResourceImpl(GraphicsAllocation &gfxAllocation) override {
        unlockResourceCalled++;
        OsAgnosticMemoryManager::unlockResourceImpl(gfxAllocation);
    }

    void waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation) override {
        waitForEnginesCompletionCalled++;
        if (waitAllocations.get()) {
            waitAllocations->addAllocation(&graphicsAllocation);
        }
        MemoryManager::waitForEnginesCompletion(graphicsAllocation);
    }

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
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override;

    bool isLimitedGPU(uint32_t rootDeviceIndex) override {
        return limitedGPU;
    }
    void forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range) { getGfxPartition(rootDeviceIndex)->init(range, 0, 0, gfxPartitions.size()); }

    bool setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) override {
        memAdviseFlags = flags;
        if (failSetMemAdvise) {
            return false;
        }
        return MemoryManager::setMemAdvise(gfxAllocation, flags, rootDeviceIndex);
    }

    bool setMemPrefetch(GraphicsAllocation *gfxAllocation, uint32_t subDeviceId, uint32_t rootDeviceIndex) override {
        memPrefetchSubDeviceId = subDeviceId;
        setMemPrefetchCalled = true;
        return MemoryManager::setMemPrefetch(gfxAllocation, subDeviceId, rootDeviceIndex);
    }

    bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) override {
        if (DebugManager.flags.UseKmdMigration.get() != -1) {
            return !!DebugManager.flags.UseKmdMigration.get();
        }
        return false;
    }

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

    uint32_t copyMemoryToAllocationBanksCalled = 0u;
    uint32_t populateOsHandlesCalled = 0u;
    uint32_t allocateGraphicsMemoryForNonSvmHostPtrCalled = 0u;
    uint32_t freeGraphicsMemoryCalled = 0u;
    uint32_t unlockResourceCalled = 0u;
    uint32_t lockResourceCalled = 0u;
    uint32_t createGraphicsAllocationFromExistingStorageCalled = 0u;
    int32_t overrideAllocateAsPackReturn = -1;
    std::vector<GraphicsAllocation *> allocationsFromExistingStorage{};
    AllocationData alignAllocationData;
    uint32_t successAllocatedGraphicsMemoryIndex = 0u;
    uint32_t maxSuccessAllocatedGraphicsMemoryIndex = std::numeric_limits<uint32_t>::max();
    std::vector<void *> lockResourcePointers;
    uint32_t handleFenceCompletionCalled = 0u;
    uint32_t waitForEnginesCompletionCalled = 0u;
    uint32_t allocateGraphicsMemoryWithPropertiesCount = 0;
    osHandle capturedSharedHandle = 0u;
    osHandle invalidSharedHandle = -1;
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
    bool failAllocateSystemMemory = false;
    bool failAllocate32Bit = false;
    bool failSetMemAdvise = false;
    bool setMemPrefetchCalled = false;
    bool cpuCopyRequired = false;
    bool forceCompressed = false;
    bool forceFailureInPrimaryAllocation = false;
    bool forceFailureInAllocationWithHostPointer = false;
    bool isMockHostMemoryManager = false;
    bool isMockEventPoolCreateMemoryManager = false;
    bool limitedGPU = false;
    bool returnFakeAllocation = false;
    bool callBasePopulateOsHandles = true;
    bool callBaseAllocateGraphicsMemoryForNonSvmHostPtr = true;
    std::unique_ptr<MockExecutionEnvironment> mockExecutionEnvironment;
    DeviceBitfield recentlyPassedDeviceBitfield{};
    std::unique_ptr<MultiGraphicsAllocation> waitAllocations = nullptr;
    uint32_t memPrefetchSubDeviceId = 0;
    MemAdviseFlags memAdviseFlags{};
    MemoryManager::AllocationStatus populateOsHandlesResult = MemoryManager::AllocationStatus::Success;
    GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtrResult = nullptr;
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

    void *allocateSystemMemory(size_t size, size_t alignment) override {
        constexpr size_t minAlignment = 16;
        alignment = std::max(alignment, minAlignment);
        return alignedMalloc(size, alignment);
    }

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
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override {
        return nullptr;
    }

    GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(std::vector<osHandle> handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override {
        return nullptr;
    }

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override {
        return nullptr;
    }
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override {
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
    GraphicsAllocation *allocateNonSystemGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
        auto allocation = baseAllocateGraphicsMemoryInDevicePool(allocationData, status);
        if (!allocation) {
            allocation = allocateGraphicsMemory(allocationData);
        }
        static_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);
        return allocation;
    }
};

class MockMemoryManagerOsAgnosticContext : public MockMemoryManager {
  public:
    MockMemoryManagerOsAgnosticContext(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
    OsContext *createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                          const EngineDescriptor &engineDescriptor) override {
        auto osContext = new OsContext(0, engineDescriptor);
        osContext->incRefInternal();
        registeredEngines.emplace_back(commandStreamReceiver, osContext);
        return osContext;
    }
};

class MockMemoryManagerWithDebuggableOsContext : public MockMemoryManager {
  public:
    MockMemoryManagerWithDebuggableOsContext(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
    OsContext *createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                          const EngineDescriptor &engineDescriptor) override {
        auto osContext = new MockOsContext(0, engineDescriptor);
        osContext->debuggableContext = true;
        osContext->incRefInternal();
        registeredEngines.emplace_back(commandStreamReceiver, osContext);
        return osContext;
    }
};

class MockMemoryManagerWithCapacity : public MockMemoryManager {
  public:
    MockMemoryManagerWithCapacity(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        if (this->capacity >= properties.size) {
            this->capacity -= properties.size;
            return MockMemoryManager::allocateGraphicsMemoryWithProperties(properties);
        }
        return nullptr;
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
        return NTHandle;
    };

    bool NTHandle = false;
};

} // namespace NEO
