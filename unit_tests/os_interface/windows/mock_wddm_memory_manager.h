/*
 * Copyright (C) 2017-2018 Intel Corporation
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
};
} // namespace OCLRT
