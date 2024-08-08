/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_residency_controller.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/utilities/spinlock.h"

namespace NEO {

WddmResidencyController::WddmResidencyController(Wddm &wddm, uint32_t osContextId) : wddm(wddm), osContextId(osContextId) {
}

void WddmResidencyController::registerCallback() {
    this->trimCallbackHandle = wddm.registerTrimCallback(WddmResidencyController::trimCallback, *this);
}

WddmResidencyController::~WddmResidencyController() {
    auto lock = this->acquireTrimCallbackLock();
    wddm.unregisterTrimCallback(WddmResidencyController::trimCallback, this->trimCallbackHandle);
    lock.unlock();

    // Wait for lock to ensure trimCallback ended
    lock.lock();
}

std::unique_lock<SpinLock> WddmResidencyController::acquireLock() {
    return std::unique_lock<SpinLock>{this->lock};
}

std::unique_lock<SpinLock> WddmResidencyController::acquireTrimCallbackLock() {
    return std::unique_lock<SpinLock>{this->trimCallbackLock};
}

WddmAllocation *WddmResidencyController::getTrimCandidateHead() {
    uint32_t i = 0;
    const size_t size = trimCandidateList.size();

    if (size == 0) {
        return nullptr;
    }
    while ((trimCandidateList[i] == nullptr) && (i < size))
        i++;

    return static_cast<WddmAllocation *>(trimCandidateList[i]);
}

void WddmResidencyController::addToTrimCandidateList(GraphicsAllocation *allocation) {
    WddmAllocation *wddmAllocation = (WddmAllocation *)allocation;
    size_t position = trimCandidateList.size();

    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    if (wddmAllocation->getTrimCandidateListPosition(this->osContextId) == trimListUnusedPosition) {
        trimCandidatesCount++;
        trimCandidateList.push_back(allocation);
        wddmAllocation->setTrimCandidateListPosition(this->osContextId, position);
    }

    checkTrimCandidateCount();
}

void WddmResidencyController::removeFromTrimCandidateList(GraphicsAllocation *allocation, bool compactList) {
    WddmAllocation *wddmAllocation = (WddmAllocation *)allocation;
    size_t position = wddmAllocation->getTrimCandidateListPosition(this->osContextId);

    DEBUG_BREAK_IF(!(trimCandidatesCount > (trimCandidatesCount - 1)));
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    trimCandidatesCount--;

    trimCandidateList[position] = nullptr;

    checkTrimCandidateCount();

    if (position == trimCandidateList.size() - 1) {
        size_t erasePosition = position;

        if (position == 0) {
            trimCandidateList.resize(0);
        } else {
            while (trimCandidateList[erasePosition] == nullptr && erasePosition > 0) {
                erasePosition--;
            }

            size_t sizeRemaining = erasePosition + 1;
            if (erasePosition == 0 && trimCandidateList[erasePosition] == nullptr) {
                sizeRemaining = 0;
            }

            trimCandidateList.resize(sizeRemaining);
        }
    }
    wddmAllocation->setTrimCandidateListPosition(this->osContextId, trimListUnusedPosition);

    if (compactList && checkTrimCandidateListCompaction()) {
        compactTrimCandidateList();
    }

    checkTrimCandidateCount();
}

void WddmResidencyController::removeFromTrimCandidateListIfUsed(WddmAllocation *allocation, bool compactList) {
    if (allocation->getTrimCandidateListPosition(this->osContextId) != trimListUnusedPosition) {
        this->removeFromTrimCandidateList(allocation, true);
    }
}

void WddmResidencyController::checkTrimCandidateCount() {
    if (debugManager.flags.ResidencyDebugEnable.get()) {
        [[maybe_unused]] uint32_t sum = 0;
        for (auto trimCandidate : trimCandidateList) {
            if (trimCandidate != nullptr) {
                sum++;
            }
        }
        DEBUG_BREAK_IF(sum != trimCandidatesCount);
    }
}

bool WddmResidencyController::checkTrimCandidateListCompaction() {
    return 2 * trimCandidatesCount <= trimCandidateList.size();
}

void WddmResidencyController::compactTrimCandidateList() {
    size_t size = trimCandidateList.size();
    size_t freePosition = 0;

    if (size == 0 || size == trimCandidatesCount) {
        return;
    }

    DEBUG_BREAK_IF(!(trimCandidateList[size - 1] != nullptr));

    [[maybe_unused]] uint32_t previousCount = trimCandidatesCount;
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    while (freePosition < trimCandidatesCount && trimCandidateList[freePosition] != nullptr)
        freePosition++;

    for (uint32_t i = 1; i < size; i++) {

        if (trimCandidateList[i] != nullptr && freePosition < i) {
            trimCandidateList[freePosition] = trimCandidateList[i];
            trimCandidateList[i] = nullptr;
            static_cast<WddmAllocation *>(trimCandidateList[freePosition])->setTrimCandidateListPosition(this->osContextId, freePosition);
            freePosition++;

            // Last element was moved, erase elements from freePosition
            if (i == size - 1) {
                trimCandidateList.resize(freePosition);
            }
        }
    }
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());
    DEBUG_BREAK_IF(trimCandidatesCount != previousCount);

    checkTrimCandidateCount();
}

void WddmResidencyController::resetMonitoredFenceParams(D3DKMT_HANDLE &handle, uint64_t *cpuAddress, D3DGPU_VIRTUAL_ADDRESS &gpuAddress) {
    monitoredFence.lastSubmittedFence = 0;
    monitoredFence.currentFenceValue = 1;
    monitoredFence.fenceHandle = handle;
    monitoredFence.cpuAddress = cpuAddress;
    monitoredFence.gpuAddress = gpuAddress;
}

/**
 * @brief Makes resident passed allocations on a device
 *
 * @param[in] allocationsForResidency container of allocations which need to be resident.
 * @param[out] requiresBlockingResidencyHandling flag indicating whether wait for paging fence must be handled in user thread.
 * Setting to false means that it can be handled in background thread, which will signal semaphore after paging fence reaches required value.
 *
 * @note This method skips allocations which are already resident.
 *
 * @return returns true if all allocations either succeeded or are pending to be resident
 */
bool WddmResidencyController::makeResidentResidencyAllocations(const ResidencyContainer &allocationsForResidency, bool &requiresBlockingResidencyHandling) {
    const size_t residencyCount = allocationsForResidency.size();
    constexpr uint32_t stackAllocations = 64;
    constexpr uint32_t stackHandlesCount = NEO::maxFragmentsCount * EngineLimits::maxHandleCount * stackAllocations;
    StackVec<D3DKMT_HANDLE, stackHandlesCount> handlesForResidency;
    uint32_t totalHandlesCount = 0;
    size_t totalSize = 0;
    requiresBlockingResidencyHandling = false;
    if (debugManager.flags.WaitForPagingFenceInController.get() != -1) {
        requiresBlockingResidencyHandling = !debugManager.flags.WaitForPagingFenceInController.get();
    }

    auto lock = this->acquireLock();

    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", this->getMonitoredFence().currentFenceValue);

    for (uint32_t i = 0; i < residencyCount; i++) {
        WddmAllocation *allocation = static_cast<WddmAllocation *>(allocationsForResidency[i]);
        ResidencyData &residencyData = allocation->getResidencyData();
        static constexpr int maxFragments = 3;
        bool fragmentResidency[maxFragments] = {false, false, false};
        totalSize += allocation->getAlignedSize();

        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation, gpu address = ", std::hex, allocation->getGpuAddress(), residencyData.resident[osContextId] ? "resident" : "not resident");

        const auto fragmentCount = allocation->fragmentsStorage.fragmentCount;
        UNRECOVERABLE_IF(fragmentCount > maxFragments);
        if (allocation->getTrimCandidateListPosition(this->osContextId) != trimListUnusedPosition) {
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation, gpu address = ", std::hex, allocation->getGpuAddress(), "on trimCandidateList");
            this->removeFromTrimCandidateList(allocation, false);
        } else {
            for (uint32_t allocationId = 0; allocationId < fragmentCount; allocationId++) {
                fragmentResidency[allocationId] = allocation->fragmentsStorage.fragmentStorageData[allocationId].residency->resident[osContextId];
                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "fragment handle =",
                        static_cast<OsHandleWin *>(allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage)->handle,
                        fragmentResidency[allocationId] ? "resident" : "not resident");
            }
        }

        if (fragmentCount > 0) {
            for (uint32_t allocationId = 0; allocationId < fragmentCount; allocationId++) {
                if (!fragmentResidency[allocationId]) {
                    handlesForResidency.push_back(static_cast<OsHandleWin *>(allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage)->handle);
                    totalHandlesCount++;
                    requiresBlockingResidencyHandling |= (allocation->getAllocationType() != AllocationType::buffer && allocation->getAllocationType() != AllocationType::bufferHostMemory);
                }
            }
        } else if (!residencyData.resident[osContextId]) {
            for (uint32_t gmmId = 0; gmmId < allocation->getNumGmms(); ++gmmId) {
                handlesForResidency.push_back(allocation->getHandle(gmmId));
                totalHandlesCount++;
                requiresBlockingResidencyHandling |= (allocation->getAllocationType() != AllocationType::buffer && allocation->getAllocationType() != AllocationType::bufferHostMemory);
            }
        }
    }

    bool result = true;
    if (totalHandlesCount) {
        uint64_t bytesToTrim = 0;
        while ((result = wddm.makeResident(&handlesForResidency[0], totalHandlesCount, false, &bytesToTrim, totalSize)) == false) {
            this->setMemoryBudgetExhausted();
            const bool trimmingDone = this->trimResidencyToBudget(bytesToTrim, lock);
            if (!trimmingDone) {
                auto evictionStatus = wddm.getTemporaryResourcesContainer()->evictAllResources();
                if (evictionStatus == MemoryOperationsStatus::success) {
                    continue;
                }
                DEBUG_BREAK_IF(evictionStatus != MemoryOperationsStatus::memoryNotFound);
                do {
                    result = wddm.makeResident(&handlesForResidency[0], totalHandlesCount, true, &bytesToTrim, totalSize);
                } while (debugManager.flags.WaitForMemoryRelease.get() && result == false);
                break;
            }
        }
    }

    if (result == true) {
        for (uint32_t i = 0; i < residencyCount; i++) {
            WddmAllocation *allocation = static_cast<WddmAllocation *>(allocationsForResidency[i]);
            // Update fence value not to early destroy / evict allocation
            const auto currentFence = this->getMonitoredFence().currentFenceValue;
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation, gpu address = ", std::hex, allocation->getGpuAddress(), "fence value to reach for eviction = ", currentFence);
            allocation->getResidencyData().updateCompletionData(currentFence, this->osContextId);
            allocation->getResidencyData().resident[osContextId] = true;

            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                auto residencyData = allocation->fragmentsStorage.fragmentStorageData[allocationId].residency;
                // Update fence value not to remove the fragment referenced by different GA in trimming callback
                residencyData->updateCompletionData(currentFence, this->osContextId);
                residencyData->resident[osContextId] = true;
            }
        }
    }

    return result;
}

void WddmResidencyController::makeNonResidentEvictionAllocations(const ResidencyContainer &evictionAllocations) {
    auto lock = this->acquireLock();
    const size_t residencyCount = evictionAllocations.size();

    for (uint32_t i = 0; i < residencyCount; i++) {
        WddmAllocation *allocation = static_cast<WddmAllocation *>(evictionAllocations[i]);
        this->addToTrimCandidateList(allocation);
    }
}

bool WddmResidencyController::isInitialized() const {
    bool requiresTrimCallbacks = OSInterface::requiresSupportForWddmTrimNotification;
    requiresTrimCallbacks = requiresTrimCallbacks && (false == debugManager.flags.DoNotRegisterTrimCallback.get());
    if (requiresTrimCallbacks) {
        return trimCallbackHandle != nullptr;
    }
    return true;
}

} // namespace NEO
