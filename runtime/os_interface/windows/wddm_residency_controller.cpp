/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_residency_controller.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

#include "runtime/utilities/spinlock.h"

namespace OCLRT {

WddmResidencyController::WddmResidencyController(Wddm &wddm, uint32_t osContextId) : wddm(wddm), osContextId(osContextId) {
    this->trimCallbackHandle = wddm.registerTrimCallback(WddmMemoryManager::trimCallback, *this);
}

WddmResidencyController::~WddmResidencyController() {
    auto lock = this->acquireTrimCallbackLock();
    wddm.unregisterTrimCallback(WddmMemoryManager::trimCallback, this->trimCallbackHandle);
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

    return reinterpret_cast<WddmAllocation *>(trimCandidateList[i]);
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
    if (DebugManager.flags.ResidencyDebugEnable.get()) {
        uint32_t sum = 0;
        for (size_t i = 0; i < trimCandidateList.size(); i++) {
            if (trimCandidateList[i] != nullptr) {
                sum++;
            }
        }
        DEBUG_BREAK_IF(sum != trimCandidatesCount);
    }
}

bool WddmResidencyController::checkTrimCandidateListCompaction() {
    if (2 * trimCandidatesCount <= trimCandidateList.size()) {
        return true;
    }
    return false;
}

void WddmResidencyController::compactTrimCandidateList() {
    size_t size = trimCandidateList.size();
    size_t freePosition = 0;

    if (size == 0 || size == trimCandidatesCount) {
        return;
    }

    DEBUG_BREAK_IF(!(trimCandidateList[size - 1] != nullptr));

    uint32_t previousCount = trimCandidatesCount;
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

void WddmResidencyController::trimResidency(D3DDDI_TRIMRESIDENCYSET_FLAGS flags, uint64_t bytes) {
    if (flags.PeriodicTrim) {
        bool periodicTrimDone = false;
        D3DKMT_HANDLE fragmentEvictHandles[3] = {0};
        uint64_t sizeToTrim = 0;
        auto lock = this->acquireLock();

        WddmAllocation *wddmAllocation = nullptr;
        while ((wddmAllocation = this->getTrimCandidateHead()) != nullptr) {

            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "lastPeriodicTrimFenceValue = ", this->getLastTrimFenceValue());

            // allocation was not used from last periodic trim
            if (wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId) <= this->getLastTrimFenceValue()) {

                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation: handle =", wddmAllocation->handle, "lastFence =", (wddmAllocation)->getResidencyData().getFenceValueForContextId(osContextId));

                uint32_t fragmentsToEvict = 0;

                if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict allocation: handle =", wddmAllocation->handle, "lastFence =", (wddmAllocation)->getResidencyData().getFenceValueForContextId(osContextId));
                    this->wddm.evict(&wddmAllocation->handle, 1, sizeToTrim);
                }

                for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                    if (wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId) <= this->getLastTrimFenceValue()) {

                        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict fragment: handle =", wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle, "lastFence =", wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId));

                        fragmentEvictHandles[fragmentsToEvict++] = wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle;
                        wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->resident = false;
                    }
                }

                if (fragmentsToEvict != 0) {
                    this->wddm.evict((D3DKMT_HANDLE *)fragmentEvictHandles, fragmentsToEvict, sizeToTrim);
                }

                wddmAllocation->getResidencyData().resident = false;

                this->removeFromTrimCandidateList(wddmAllocation, false);
            } else {
                periodicTrimDone = true;
                break;
            }
        }

        if (this->checkTrimCandidateListCompaction()) {
            this->compactTrimCandidateList();
        }
    }

    if (flags.TrimToBudget) {
        auto lock = this->acquireLock();
        trimResidencyToBudget(bytes);
    }

    if (flags.PeriodicTrim || flags.RestartPeriodicTrim) {
        const auto newPeriodicTrimFenceValue = *this->getMonitoredFence().cpuAddress;
        this->setLastTrimFenceValue(newPeriodicTrimFenceValue);
        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "updated lastPeriodicTrimFenceValue =", newPeriodicTrimFenceValue);
    }
}

bool WddmResidencyController::trimResidencyToBudget(uint64_t bytes) {
    bool trimToBudgetDone = false;
    D3DKMT_HANDLE fragmentEvictHandles[3] = {0};
    uint64_t numberOfBytesToTrim = bytes;
    WddmAllocation *wddmAllocation = nullptr;

    trimToBudgetDone = (numberOfBytesToTrim == 0);

    while (!trimToBudgetDone) {
        uint64_t lastFence = 0;
        wddmAllocation = this->getTrimCandidateHead();

        if (wddmAllocation == nullptr) {
            break;
        }

        lastFence = wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId);
        auto &monitoredFence = this->getMonitoredFence();

        if (lastFence <= monitoredFence.lastSubmittedFence) {
            uint32_t fragmentsToEvict = 0;
            uint64_t sizeEvicted = 0;
            uint64_t sizeToTrim = 0;

            if (lastFence > *monitoredFence.cpuAddress) {
                this->wddm.waitFromCpu(lastFence, this->getMonitoredFence());
            }

            if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                this->wddm.evict(&wddmAllocation->handle, 1, sizeToTrim);

                sizeEvicted = wddmAllocation->getAlignedSize();
            } else {
                auto &fragmentStorageData = wddmAllocation->fragmentsStorage.fragmentStorageData;
                for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                    if (fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId) <= monitoredFence.lastSubmittedFence) {
                        fragmentEvictHandles[fragmentsToEvict++] = fragmentStorageData[allocationId].osHandleStorage->handle;
                    }
                }

                if (fragmentsToEvict != 0) {
                    this->wddm.evict((D3DKMT_HANDLE *)fragmentEvictHandles, fragmentsToEvict, sizeToTrim);

                    for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                        if (fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId) <= monitoredFence.lastSubmittedFence) {
                            fragmentStorageData[allocationId].residency->resident = false;
                            sizeEvicted += fragmentStorageData[allocationId].fragmentSize;
                        }
                    }
                }
            }

            if (sizeEvicted >= numberOfBytesToTrim) {
                numberOfBytesToTrim = 0;
            } else {
                numberOfBytesToTrim -= sizeEvicted;
            }

            wddmAllocation->getResidencyData().resident = false;
            this->removeFromTrimCandidateList(wddmAllocation, false);
            trimToBudgetDone = (numberOfBytesToTrim == 0);
        } else {
            trimToBudgetDone = true;
        }
    }

    if (bytes > numberOfBytesToTrim && this->checkTrimCandidateListCompaction()) {
        this->compactTrimCandidateList();
    }

    return numberOfBytesToTrim == 0;
}

} // namespace OCLRT
