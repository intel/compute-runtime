/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_host_ptr_manager.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

namespace NEO {
class MockWddmMemoryManager : public MemoryManagerCreate<WddmMemoryManager> {
    using BaseClass = WddmMemoryManager;

  public:
    using BaseClass::allocateGraphicsMemory64kb;
    using BaseClass::allocateGraphicsMemoryForNonSvmHostPtr;
    using BaseClass::allocateGraphicsMemoryInDevicePool;
    using BaseClass::allocateGraphicsMemoryWithProperties;
    using BaseClass::allocateShareableMemory;
    using BaseClass::createGraphicsAllocation;
    using BaseClass::createWddmAllocation;
    using BaseClass::getWddm;
    using BaseClass::gfxPartitions;
    using BaseClass::localMemorySupported;
    using BaseClass::supportsMultiStorageResources;
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
    GraphicsAllocation *allocate32BitGraphicsMemory(uint32_t rootDeviceIndex, size_t size, const void *ptr, GraphicsAllocation::AllocationType allocationType) {
        bool allocateMemory = ptr == nullptr;
        AllocationData allocationData;
        MockAllocationProperties properties(rootDeviceIndex, allocateMemory, size, allocationType);
        getAllocationData(allocationData, properties, ptr, createStorageInfoFromProperties(properties));
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }

    void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) override {
        freeGraphicsMemoryImplCalled++;
        BaseClass::freeGraphicsMemoryImpl(gfxAllocation);
    }

    uint32_t freeGraphicsMemoryImplCalled = 0u;
};
} // namespace NEO
