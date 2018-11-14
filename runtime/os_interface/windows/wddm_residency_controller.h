/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/memory_manager/residency_container.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "runtime/os_interface/windows/windows_defs.h"
#include "runtime/utilities/spinlock.h"

#include <atomic>
#include <mutex>

namespace OCLRT {

class GraphicsAllocation;
class WddmAllocation;
class Wddm;

class WddmResidencyController {
  public:
    WddmResidencyController(Wddm &wddm, uint32_t osContextId);
    MOCKABLE_VIRTUAL ~WddmResidencyController();

    MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock();
    std::unique_lock<SpinLock> acquireTrimCallbackLock();

    WddmAllocation *getTrimCandidateHead();
    void addToTrimCandidateList(GraphicsAllocation *allocation);
    void removeFromTrimCandidateList(GraphicsAllocation *allocation, bool compactList);
    void removeFromTrimCandidateListIfUsed(WddmAllocation *allocation, bool compactList);
    void checkTrimCandidateCount();

    bool checkTrimCandidateListCompaction();
    void compactTrimCandidateList();

    uint64_t getLastTrimFenceValue() { return lastTrimFenceValue; }
    void setLastTrimFenceValue(uint64_t value) { lastTrimFenceValue = value; }
    const ResidencyContainer &peekTrimCandidateList() const { return trimCandidateList; }
    uint32_t peekTrimCandidatesCount() const { return trimCandidatesCount; }

    MonitoredFence &getMonitoredFence() { return monitoredFence; }
    void resetMonitoredFenceParams(D3DKMT_HANDLE &handle, uint64_t *cpuAddress, D3DGPU_VIRTUAL_ADDRESS &gpuAddress);

    void registerCallback();

    void trimResidency(D3DDDI_TRIMRESIDENCYSET_FLAGS flags, uint64_t bytes);
    bool trimResidencyToBudget(uint64_t bytes);

    bool isMemoryBudgetExhausted() const { return memoryBudgetExhausted; }
    void setMemoryBudgetExhausted() { memoryBudgetExhausted = true; }

    bool makeResidentResidencyAllocations(ResidencyContainer &allocationsForResidency);
    void makeNonResidentEvictionAllocations(ResidencyContainer &evictionAllocations);

  protected:
    Wddm &wddm;
    uint32_t osContextId;
    MonitoredFence monitoredFence = {};

    SpinLock lock;
    SpinLock trimCallbackLock;

    bool memoryBudgetExhausted = false;
    uint64_t lastTrimFenceValue = 0u;
    ResidencyContainer trimCandidateList;
    uint32_t trimCandidatesCount = 0;

    VOID *trimCallbackHandle = nullptr;
};
} // namespace OCLRT
