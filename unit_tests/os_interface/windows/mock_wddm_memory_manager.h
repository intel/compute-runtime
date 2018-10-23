/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"

namespace OCLRT {
class MockWddmMemoryManager : public WddmMemoryManager {
    using BaseClass = WddmMemoryManager;

  public:
    using BaseClass::createWddmAllocation;
    using BaseClass::trimCallbackHandle;
    using BaseClass::trimResidency;
    using BaseClass::trimResidencyToBudget;
    using BaseClass::WddmMemoryManager;

    MockWddmMemoryManager(Wddm *wddm, ExecutionEnvironment &executionEnvironment) : WddmMemoryManager(false, false, wddm, executionEnvironment){};
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
