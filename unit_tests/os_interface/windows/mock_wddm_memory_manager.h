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
    using BaseClass::addToTrimCandidateList;
    using BaseClass::checkTrimCandidateListCompaction;
    using BaseClass::compactTrimCandidateList;
    using BaseClass::createWddmAllocation;
    using BaseClass::lastPeriodicTrimFenceValue;
    using BaseClass::removeFromTrimCandidateList;
    using BaseClass::trimCandidateList;
    using BaseClass::trimCandidatesCount;
    using BaseClass::trimResidency;
    using BaseClass::trimResidencyToBudget;

    MockWddmMemoryManager(bool enable64kbPages, bool enableLocalMemory, Wddm *wddm) : WddmMemoryManager(enable64kbPages, enableLocalMemory, wddm){};
    MockWddmMemoryManager(Wddm *wddm) : WddmMemoryManager(false, false, wddm){};
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
