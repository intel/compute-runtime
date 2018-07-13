/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
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

    MockWddmMemoryManager(bool enable64kbPages, Wddm *wddm) : WddmMemoryManager(enable64kbPages, wddm){};
    MockWddmMemoryManager(Wddm *wddm) : WddmMemoryManager(false, wddm){};
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
