/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_host_ptr_manager.h"

#include "gmock/gmock.h"

namespace NEO {

template <class T>
class MemoryManagerCreate : public T {
  public:
    using T::T;

    template <class... U>
    MemoryManagerCreate(bool enable64kbPages, bool enableLocalMemory, U &&... args) : T(std::forward<U>(args)...) {
        this->enable64kbpages = enable64kbPages;
        this->localMemorySupported = enableLocalMemory;
    }
};

class MockMemoryManager : public MemoryManagerCreate<OsAgnosticMemoryManager> {
  public:
    using MemoryManager::allocateGraphicsMemoryForNonSvmHostPtr;
    using MemoryManager::allocateGraphicsMemoryInPreferredPool;
    using MemoryManager::allocateGraphicsMemoryWithProperties;
    using MemoryManager::AllocationData;
    using MemoryManager::createGraphicsAllocation;
    using MemoryManager::getAllocationData;
    using MemoryManager::multiContextResourceDestructor;
    using MemoryManager::registeredEngines;
    using MemoryManager::useInternal32BitAllocator;
    using OsAgnosticMemoryManager::allocateGraphicsMemoryForImageFromHostPtr;
    using MemoryManagerCreate<OsAgnosticMemoryManager>::MemoryManagerCreate;

    MockMemoryManager(ExecutionEnvironment &executionEnvironment) : MockMemoryManager(false, executionEnvironment) {}

    MockMemoryManager(bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, enableLocalMemory, executionEnvironment) {
        hostPtrManager.reset(new MockHostPtrManager);
    };

    MockMemoryManager() : MockMemoryManager(*(new MockExecutionEnvironment(*platformDevices))) {
        mockExecutionEnvironment.reset(&executionEnvironment);
    };
    MockMemoryManager(bool enable64pages, bool enableLocalMemory) : MemoryManagerCreate(enable64pages, enableLocalMemory, *(new MockExecutionEnvironment(*platformDevices))) {
        mockExecutionEnvironment.reset(&executionEnvironment);
    }
    GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) override;
    void setDeferredDeleter(DeferredDeleter *deleter);
    void overrideAsyncDeleterFlag(bool newValue);
    GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override;
    int redundancyRatio = 1;

    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override;
    GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override;
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override;

    void *allocateSystemMemory(size_t size, size_t alignment) override;

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
        freeGraphicsMemoryCalled++;
        OsAgnosticMemoryManager::freeGraphicsMemoryImpl(gfxAllocation);
    };

    void *lockResourceImpl(GraphicsAllocation &gfxAllocation) override {
        lockResourceCalled++;
        return OsAgnosticMemoryManager::lockResourceImpl(gfxAllocation);
    }

    void unlockResourceImpl(GraphicsAllocation &gfxAllocation) override {
        unlockResourceCalled++;
        OsAgnosticMemoryManager::unlockResourceImpl(gfxAllocation);
    }

    void handleFenceCompletion(GraphicsAllocation *graphicsAllocation) override {
        handleFenceCompletionCalled++;
        OsAgnosticMemoryManager::handleFenceCompletion(graphicsAllocation);
    }

    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType);

    uint32_t freeGraphicsMemoryCalled = 0u;
    uint32_t unlockResourceCalled = 0u;
    uint32_t lockResourceCalled = 0u;
    uint32_t handleFenceCompletionCalled = 0u;
    bool allocationCreated = false;
    bool allocation64kbPageCreated = false;
    bool allocationInDevicePoolCreated = false;
    bool failInDevicePool = false;
    bool failInDevicePoolWithError = false;
    bool failInAllocateWithSizeAndAlignment = false;
    bool preferRenderCompressedFlagPassed = false;
    bool allocateForImageCalled = false;
    std::unique_ptr<ExecutionEnvironment> mockExecutionEnvironment;
};

using AllocationData = MockMemoryManager::AllocationData;

class GMockMemoryManager : public MockMemoryManager {
  public:
    GMockMemoryManager(const ExecutionEnvironment &executionEnvironment) : MockMemoryManager(const_cast<ExecutionEnvironment &>(executionEnvironment)){};
    MOCK_METHOD1(populateOsHandles, MemoryManager::AllocationStatus(OsHandleStorage &handleStorage));
    MOCK_METHOD1(allocateGraphicsMemoryForNonSvmHostPtr, GraphicsAllocation *(const AllocationData &));

    MemoryManager::AllocationStatus MemoryManagerPopulateOsHandles(OsHandleStorage &handleStorage) { return OsAgnosticMemoryManager::populateOsHandles(handleStorage); }
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
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) override {
        return nullptr;
    }
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override {
        return nullptr;
    }

    void *lockResourceImpl(GraphicsAllocation &gfxAllocation) override { return nullptr; };
    void unlockResourceImpl(GraphicsAllocation &gfxAllocation) override{};

    MemoryManager::AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) override {
        return AllocationStatus::Error;
    };
    void cleanOsHandles(OsHandleStorage &handleStorage) override{};

    uint64_t getSystemSharedMemory() override {
        return 0;
    };

    uint64_t getMaxApplicationAddress() override {
        return MemoryConstants::max32BitAppAddress;
    };

    GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) override {
        return nullptr;
    };
    GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override {
        return nullptr;
    }
    int32_t failedAllocationsCount = 0;
};

class GMockMemoryManagerFailFirstAllocation : public MockMemoryManager {
  public:
    GMockMemoryManagerFailFirstAllocation(bool enableLocalMemory, const ExecutionEnvironment &executionEnvironment) : MockMemoryManager(enableLocalMemory, const_cast<ExecutionEnvironment &>(executionEnvironment)){};
    GMockMemoryManagerFailFirstAllocation(const ExecutionEnvironment &executionEnvironment) : GMockMemoryManagerFailFirstAllocation(false, executionEnvironment){};

    MOCK_METHOD2(allocateGraphicsMemoryInDevicePool, GraphicsAllocation *(const AllocationData &, AllocationStatus &));
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

} // namespace NEO
