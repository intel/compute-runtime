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
 * @note This method filters allocations which are already resident. After calling this method, passed allocationsForResidency will contain
 * only allocations which were not resident before.
 *
 * @return returns true if all allocations either succeeded or are pending to be resident
 */
bool WddmResidencyController::makeResidentResidencyAllocations(ResidencyContainer &allocationsForResidency, bool &requiresBlockingResidencyHandling) {
    constexpr uint32_t stackAllocations = 64;
    constexpr uint32_t stackHandlesCount = NEO::maxFragmentsCount * EngineLimits::maxHandleCount * stackAllocations;
    StackVec<D3DKMT_HANDLE, stackHandlesCount> handlesForResidency;
    uint32_t totalHandlesCount = 0;
    size_t totalSize = 0;
    const auto currentFence = this->getMonitoredFence().currentFenceValue;
    requiresBlockingResidencyHandling = false;
    if (debugManager.flags.WaitForPagingFenceInController.get() != -1) {
        requiresBlockingResidencyHandling = !debugManager.flags.WaitForPagingFenceInController.get();
    }

    auto lock = this->acquireLock();

    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", this->getMonitoredFence().currentFenceValue);
    auto iter = allocationsForResidency.begin();
    while (iter != allocationsForResidency.end()) {
        WddmAllocation *allocation = static_cast<WddmAllocation *>(*iter);
        ResidencyData &residencyData = allocation->getResidencyData();
        static constexpr int maxFragments = 3;
        bool fragmentResidency[maxFragments] = {false, false, false};
        const auto fragmentCount = allocation->fragmentsStorage.fragmentCount;
        UNRECOVERABLE_IF(fragmentCount > maxFragments);

        totalSize += allocation->getAlignedSize();

        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation, gpu address = ", std::hex, allocation->getGpuAddress(), residencyData.resident[osContextId] ? "resident" : "not resident");
        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation, gpu address = ", std::hex, allocation->getGpuAddress(), "fence value to reach for eviction = ", currentFence);

        allocation->getResidencyData().updateCompletionData(currentFence, this->osContextId);
        bool isAlreadyResident = true;
        if (fragmentCount > 0) {
            for (uint32_t allocationId = 0; allocationId < fragmentCount; allocationId++) {
                auto residencyData = allocation->fragmentsStorage.fragmentStorageData[allocationId].residency;
                residencyData->updateCompletionData(currentFence, this->osContextId);
                if (!fragmentResidency[allocationId]) {
                    handlesForResidency.push_back(static_cast<OsHandleWin *>(allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage)->handle);
                    totalHandlesCount++;
                    requiresBlockingResidencyHandling |= (allocation->getAllocationType() != AllocationType::buffer && allocation->getAllocationType() != AllocationType::bufferHostMemory);
                    isAlreadyResident = false;
                }
            }
        } else if (!residencyData.resident[osContextId]) {
            for (uint32_t gmmId = 0; gmmId < allocation->getNumGmms(); ++gmmId) {
                handlesForResidency.push_back(allocation->getHandle(gmmId));
                totalHandlesCount++;
                requiresBlockingResidencyHandling |= (allocation->getAllocationType() != AllocationType::buffer && allocation->getAllocationType() != AllocationType::bufferHostMemory);
                isAlreadyResident = false;
            }
        }

        if (isAlreadyResident) {
            iter = allocationsForResidency.erase(iter);
        } else {
            iter++;
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
        iter = allocationsForResidency.begin();
        while (iter != allocationsForResidency.end()) {
            WddmAllocation *allocation = static_cast<WddmAllocation *>(*iter);
            allocation->getResidencyData().resident[osContextId] = true;
            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                auto residencyData = allocation->fragmentsStorage.fragmentStorageData[allocationId].residency;
                residencyData->resident[osContextId] = true;
            }
            iter++;
        }
    }

    return result;
}

bool WddmResidencyController::isInitialized() const {
    bool requiresTrimCallbacks = OSInterface::requiresSupportForWddmTrimNotification;
    requiresTrimCallbacks = requiresTrimCallbacks && (false == debugManager.flags.DoNotRegisterTrimCallback.get());
    if (requiresTrimCallbacks) {
        return trimCallbackHandle != nullptr;
    }
    return true;
}

void WddmResidencyController::setCommandStreamReceiver(CommandStreamReceiver *csr) {
    this->csr = csr;
}

} // namespace NEO
