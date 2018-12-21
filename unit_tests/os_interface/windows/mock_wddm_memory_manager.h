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

namespace OCLRT {
class MockWddmMemoryManager : public WddmMemoryManager {
    using BaseClass = WddmMemoryManager;

  public:
    using BaseClass::allocateGraphicsMemory;
    using BaseClass::allocateGraphicsMemory64kb;
    using BaseClass::allocateGraphicsMemoryInDevicePool;
    using BaseClass::createWddmAllocation;
    using BaseClass::WddmMemoryManager;

    MockWddmMemoryManager(Wddm *wddm, ExecutionEnvironment &executionEnvironment) : WddmMemoryManager(false, false, wddm, executionEnvironment) {
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
    GraphicsAllocation *allocate32BitGraphicsMemory(size_t size, const void *ptr, AllocationOrigin allocationOrigin) {
        bool allocateMemory = ptr == nullptr;
        AllocationData allocationData;
        if (allocationOrigin == AllocationOrigin::EXTERNAL_ALLOCATION) {
            getAllocationData(allocationData, MockAllocationProperties::getPropertiesFor32BitExternalAllocation(size, allocateMemory), 0u, ptr);
        } else {
            getAllocationData(allocationData, MockAllocationProperties::getPropertiesFor32BitInternalAllocation(size, allocateMemory), 0u, ptr);
        }
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }
};
} // namespace OCLRT
