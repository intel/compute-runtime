/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/os_interface/windows/windows_wrapper.h"
#include "core/utilities/spinlock.h"
#include "runtime/memory_manager/residency_container.h"
#include "runtime/os_interface/windows/windows_defs.h"

#include <atomic>
#include <mutex>

namespace NEO {

class GraphicsAllocation;
class WddmAllocation;
class Wddm;

class WddmResidencyController {
  public:
    WddmResidencyController(Wddm &wddm, uint32_t osContextId);
    MOCKABLE_VIRTUAL ~WddmResidencyController();

    static void APIENTRY trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification);

    MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock();
    std::unique_lock<SpinLock> acquireTrimCallbackLock();

    WddmAllocation *getTrimCandidateHead();
    void addToTrimCandidateList(GraphicsAllocation *allocation);
    void removeFromTrimCandidateList(GraphicsAllocation *allocation, bool compactList);
    void removeFromTrimCandidateListIfUsed(WddmAllocation *allocation, bool compactList);
    void checkTrimCandidateCount();

    bool checkTrimCandidateListCompaction();
    void compactTrimCandidateList();

    bool wasAllocationUsedSinceLastTrim(uint64_t fenceValue) { return fenceValue > lastTrimFenceValue; }
    void updateLastTrimFenceValue() { lastTrimFenceValue = *this->getMonitoredFence().cpuAddress; }
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
} // namespace NEO
