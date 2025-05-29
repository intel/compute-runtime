/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_residency_controller.h"

#include "shared/source/command_stream/command_stream_receiver.h"
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
    const size_t residencyCount = allocationsForResidency.size();
    requiresBlockingResidencyHandling = false;
    if (debugManager.flags.WaitForPagingFenceInController.get() != -1) {
        requiresBlockingResidencyHandling = !debugManager.flags.WaitForPagingFenceInController.get();
    }

    auto lock = this->acquireLock();
    backupResidencyContainer = allocationsForResidency;
    auto totalSize = fillHandlesContainer(allocationsForResidency, requiresBlockingResidencyHandling);

    bool result = true;
    if (!handlesForResidency.empty()) {
        uint64_t bytesToTrim = 0;
        while ((result = wddm.makeResident(handlesForResidency.data(), static_cast<uint32_t>(handlesForResidency.size()), false, &bytesToTrim, totalSize)) == false) {
            this->setMemoryBudgetExhausted();
            const bool trimmingDone = this->trimResidencyToBudget(bytesToTrim);
            allocationsForResidency = backupResidencyContainer;
            if (!trimmingDone) {
                auto evictionStatus = wddm.getTemporaryResourcesContainer()->evictAllResources();
                totalSize = fillHandlesContainer(allocationsForResidency, requiresBlockingResidencyHandling);
                if (evictionStatus == MemoryOperationsStatus::success) {
                    continue;
                }
                DEBUG_BREAK_IF(evictionStatus != MemoryOperationsStatus::memoryNotFound);
                do {
                    result = wddm.makeResident(handlesForResidency.data(), static_cast<uint32_t>(handlesForResidency.size()), true, &bytesToTrim, totalSize);
                } while (debugManager.flags.WaitForMemoryRelease.get() && result == false);
                break;
            }
            totalSize = fillHandlesContainer(allocationsForResidency, requiresBlockingResidencyHandling);
        }
    }
    const auto currentFence = this->getMonitoredFence().currentFenceValue;

    if (result == true) {
        for (uint32_t i = 0; i < residencyCount; i++) {
            WddmAllocation *allocation = static_cast<WddmAllocation *>(backupResidencyContainer[i]);
            allocation->getResidencyData().resident[osContextId] = true;
            allocation->getResidencyData().updateCompletionData(currentFence, this->osContextId);
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation, gpu address = ", std::hex, allocation->getGpuAddress(), "fence value to reach for eviction = ", currentFence);
            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                auto residencyData = allocation->fragmentsStorage.fragmentStorageData[allocationId].residency;
                residencyData->resident[osContextId] = true;
                residencyData->updateCompletionData(currentFence, this->osContextId);
            }
        }
    } else {
        allocationsForResidency.clear();
    }

    return result;
}

/**
 * @brief Fills containers related to residency handling based on passed allocations. For each allocation,
 * if it's not yet resident, this allocation and it's handle are added to associated containers.
 *
 * @return returns total size in bytes of allocations which are not yet resident.
 */
size_t WddmResidencyController::fillHandlesContainer(ResidencyContainer &allocationsForResidency, bool &requiresBlockingResidencyHandling) {
    size_t totalSize = 0;
    const size_t residencyCount = allocationsForResidency.size();
    handlesForResidency.clear();
    handlesForResidency.reserve(residencyCount);

    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", this->getMonitoredFence().currentFenceValue);

    auto checkIfAlreadyResident = [&](GraphicsAllocation *alloc) {
        WddmAllocation *allocation = static_cast<WddmAllocation *>(alloc);
        ResidencyData &residencyData = allocation->getResidencyData();
        static constexpr int maxFragments = 3;
        const auto fragmentCount = allocation->fragmentsStorage.fragmentCount;
        UNRECOVERABLE_IF(fragmentCount > maxFragments);

        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation, gpu address = ", std::hex, allocation->getGpuAddress(), residencyData.resident[osContextId] ? "resident" : "not resident");
        bool isAlreadyResident = true;
        if (fragmentCount > 0) {
            for (uint32_t allocationId = 0; allocationId < fragmentCount; allocationId++) {
                handlesForResidency.push_back(static_cast<OsHandleWin *>(allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage)->handle);
                requiresBlockingResidencyHandling |= (allocation->getAllocationType() != AllocationType::buffer && allocation->getAllocationType() != AllocationType::bufferHostMemory);
                isAlreadyResident = false;
            }
        } else if (!residencyData.resident[osContextId]) {
            for (uint32_t gmmId = 0; gmmId < allocation->getNumGmms(); ++gmmId) {
                handlesForResidency.push_back(allocation->getHandle(gmmId));
                requiresBlockingResidencyHandling |= (allocation->getAllocationType() != AllocationType::buffer && allocation->getAllocationType() != AllocationType::bufferHostMemory);
                isAlreadyResident = false;
            }
        }

        if (!isAlreadyResident) {
            totalSize += allocation->getAlignedSize();
        }
        return isAlreadyResident;
    };

    allocationsForResidency.erase(std::remove_if(allocationsForResidency.begin(), allocationsForResidency.end(), checkIfAlreadyResident), allocationsForResidency.end());
    return totalSize;
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

void WddmResidencyController::removeAllocation(ResidencyContainer &container, GraphicsAllocation *gfxAllocation) {
    std::unique_lock<std::mutex> lock1(this->lock, std::defer_lock);
    std::unique_lock<std::mutex> lock2(this->trimCallbackLock, std::defer_lock);
    std::lock(lock1, lock2);

    auto iter = std::find(container.begin(), container.end(), gfxAllocation);
    if (iter != container.end()) {
        container.erase(iter);
    }
}

} // namespace NEO
