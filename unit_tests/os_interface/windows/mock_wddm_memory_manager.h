/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"
#include "unit_tests/mocks/mock_host_ptr_manager.h"
#include "unit_tests/mocks/mock_memory_manager.h"

namespace NEO {
class MockWddmMemoryManager : public MemoryManagerCreate<WddmMemoryManager> {
    using BaseClass = WddmMemoryManager;

  public:
    using BaseClass::allocateGraphicsMemory64kb;
    using BaseClass::allocateGraphicsMemoryForNonSvmHostPtr;
    using BaseClass::allocateGraphicsMemoryInDevicePool;
    using BaseClass::allocateGraphicsMemoryWithProperties;
    using BaseClass::createGraphicsAllocation;
    using BaseClass::createWddmAllocation;
    using BaseClass::gfxPartition;
    using BaseClass::localMemorySupported;
    using MemoryManagerCreate<WddmMemoryManager>::MemoryManagerCreate;

    MockWddmMemoryManager(ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(false, false, executionEnvironment) {
        hostPtrManager.reset(new MockHostPtrManager);
    };
    void setDeferredDeleter(DeferredDeleter *deleter) {
        this->deferredDeleter.reset(deleter);
    }
    void setForce32bitAllocations(bool newValue) {
        this->force32bitAllocations = newValue;
    }
    bool validateAllocationMock(WddmAllocation *graphicsAllocation) {
        return this->validateAllocation(graphicsAllocation);
    }
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType) {
        bool allocateMemory = ptr == nullptr;
        AllocationData allocationData;
        getAllocationData(allocationData, MockAllocationProperties(allocateMemory, size, allocationType), {}, ptr);
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
        freeGraphicsMemoryImplCalled++;
        BaseClass::freeGraphicsMemoryImpl(gfxAllocation);
    }

    uint32_t freeGraphicsMemoryImplCalled = 0u;
};
} // namespace NEO
