/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/os_interface/windows/windows_defs.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/utilities/spinlock.h"

#include <atomic>
#include <mutex>

struct _D3DKMT_TRIMNOTIFICATION;
typedef _D3DKMT_TRIMNOTIFICATION D3DKMT_TRIMNOTIFICATION;
struct D3DDDI_TRIMRESIDENCYSET_FLAGS;

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

    MOCKABLE_VIRTUAL bool checkTrimCandidateListCompaction();
    void compactTrimCandidateList();

    bool wasAllocationUsedSinceLastTrim(uint64_t fenceValue) { return fenceValue > lastTrimFenceValue; }
    void updateLastTrimFenceValue() { lastTrimFenceValue = *this->getMonitoredFence().cpuAddress; }
    const ResidencyContainer &peekTrimCandidateList() const { return trimCandidateList; }
    uint32_t peekTrimCandidatesCount() const { return trimCandidatesCount; }

    MonitoredFence &getMonitoredFence() { return monitoredFence; }
    void resetMonitoredFenceParams(D3DKMT_HANDLE &handle, uint64_t *cpuAddress, D3DGPU_VIRTUAL_ADDRESS &gpuAddress);

    void registerCallback();

    void trimResidency(const D3DDDI_TRIMRESIDENCYSET_FLAGS &flags, uint64_t bytes);
    bool trimResidencyToBudget(uint64_t bytes);

    bool isMemoryBudgetExhausted() const { return memoryBudgetExhausted; }
    void setMemoryBudgetExhausted() { memoryBudgetExhausted = true; }

    bool makeResidentResidencyAllocations(const ResidencyContainer &allocationsForResidency);
    void makeNonResidentEvictionAllocations(const ResidencyContainer &evictionAllocations);

    bool isInitialized() const;

  protected:
    MonitoredFence monitoredFence = {};

    ResidencyContainer trimCandidateList;

    SpinLock lock;
    SpinLock trimCallbackLock;

    uint64_t lastTrimFenceValue = 0u;

    Wddm &wddm;
    VOID *trimCallbackHandle = nullptr;

    uint32_t osContextId;
    uint32_t trimCandidatesCount = 0;

    bool memoryBudgetExhausted = false;
};
} // namespace NEO
