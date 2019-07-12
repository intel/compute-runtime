/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_residency_controller.h"

#include "core/utilities/spinlock.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"
#include "runtime/os_interface/windows/wddm_residency_allocations_container.h"

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

void APIENTRY WddmResidencyController::trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification) {
    auto residencyController = static_cast<WddmResidencyController *>(trimNotification->Context);
    DEBUG_BREAK_IF(residencyController == nullptr);

    auto lock = residencyController->acquireTrimCallbackLock();
    residencyController->trimResidency(trimNotification->Flags, trimNotification->NumBytesToTrim);
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
    return 2 * trimCandidatesCount <= trimCandidateList.size();
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
        D3DKMT_HANDLE fragmentEvictHandles[3] = {0};
        uint64_t sizeToTrim = 0;
        auto lock = this->acquireLock();

        WddmAllocation *wddmAllocation = nullptr;
        while ((wddmAllocation = this->getTrimCandidateHead()) != nullptr) {

            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "lastPeriodicTrimFenceValue = ", lastTrimFenceValue);

            if (wasAllocationUsedSinceLastTrim(wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId))) {
                break;
            }
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation: default handle =", wddmAllocation->getDefaultHandle(), "lastFence =", (wddmAllocation)->getResidencyData().getFenceValueForContextId(osContextId));

            uint32_t fragmentsToEvict = 0;

            if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict allocation: default handle =", wddmAllocation->getDefaultHandle(), "lastFence =", (wddmAllocation)->getResidencyData().getFenceValueForContextId(osContextId));
                this->wddm.evict(wddmAllocation->getHandles().data(), wddmAllocation->getNumHandles(), sizeToTrim);
            }

            for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                AllocationStorageData &fragmentStorageData = wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId];
                if (!wasAllocationUsedSinceLastTrim(fragmentStorageData.residency->getFenceValueForContextId(osContextId))) {
                    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict fragment: handle =", wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle, "lastFence =", wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId));
                    fragmentEvictHandles[fragmentsToEvict++] = fragmentStorageData.osHandleStorage->handle;
                    fragmentStorageData.residency->resident[osContextId] = false;
                }
            }
            if (fragmentsToEvict != 0) {
                this->wddm.evict((D3DKMT_HANDLE *)fragmentEvictHandles, fragmentsToEvict, sizeToTrim);
            }
            wddmAllocation->getResidencyData().resident[osContextId] = false;

            this->removeFromTrimCandidateList(wddmAllocation, false);
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
        this->updateLastTrimFenceValue();
        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "updated lastPeriodicTrimFenceValue =", lastTrimFenceValue);
    }
}

bool WddmResidencyController::trimResidencyToBudget(uint64_t bytes) {
    D3DKMT_HANDLE fragmentEvictHandles[maxFragmentsCount] = {0};
    uint64_t numberOfBytesToTrim = bytes;
    WddmAllocation *wddmAllocation = nullptr;

    while (numberOfBytesToTrim > 0 && (wddmAllocation = this->getTrimCandidateHead()) != nullptr) {
        uint64_t lastFence = wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId);
        auto &monitoredFence = this->getMonitoredFence();

        if (lastFence > monitoredFence.lastSubmittedFence) {
            break;
        }

        uint32_t fragmentsToEvict = 0;
        uint64_t sizeEvicted = 0;
        uint64_t sizeToTrim = 0;

        if (lastFence > *monitoredFence.cpuAddress) {
            this->wddm.waitFromCpu(lastFence, this->getMonitoredFence());
        }

        if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
            this->wddm.evict(wddmAllocation->getHandles().data(), wddmAllocation->getNumHandles(), sizeToTrim);
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
                        fragmentStorageData[allocationId].residency->resident[osContextId] = false;
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

        wddmAllocation->getResidencyData().resident[osContextId] = false;
        this->removeFromTrimCandidateList(wddmAllocation, false);
    }

    if (bytes > numberOfBytesToTrim && this->checkTrimCandidateListCompaction()) {
        this->compactTrimCandidateList();
    }

    return numberOfBytesToTrim == 0;
}

bool WddmResidencyController::makeResidentResidencyAllocations(ResidencyContainer &allocationsForResidency) {
    const size_t residencyCount = allocationsForResidency.size();
    std::unique_ptr<D3DKMT_HANDLE[]> handlesForResidency(new D3DKMT_HANDLE[residencyCount * maxFragmentsCount]);
    uint32_t totalHandlesCount = 0;

    auto lock = this->acquireLock();

    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "currentFenceValue =", this->getMonitoredFence().currentFenceValue);

    for (uint32_t i = 0; i < residencyCount; i++) {
        WddmAllocation *allocation = static_cast<WddmAllocation *>(allocationsForResidency[i]);
        ResidencyData &residencyData = allocation->getResidencyData();
        bool fragmentResidency[3] = {false, false, false};

        DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation =", allocation, residencyData.resident[osContextId] ? "resident" : "not resident");

        if (allocation->getTrimCandidateListPosition(this->osContextId) != trimListUnusedPosition) {
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation =", allocation, "on trimCandidateList");
            this->removeFromTrimCandidateList(allocation, false);
        } else {
            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                fragmentResidency[allocationId] = allocation->fragmentsStorage.fragmentStorageData[allocationId].residency->resident[osContextId];
                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "fragment handle =", allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle, fragmentResidency[allocationId] ? "resident" : "not resident");
            }
        }

        if (allocation->fragmentsStorage.fragmentCount > 0) {
            for (uint32_t allocationId = 0; allocationId < allocation->fragmentsStorage.fragmentCount; allocationId++) {
                if (!fragmentResidency[allocationId])
                    handlesForResidency[totalHandlesCount++] = allocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage->handle;
            }
        } else if (!residencyData.resident[osContextId]) {
            memcpy_s(&handlesForResidency[totalHandlesCount], allocation->getNumHandles() * sizeof(D3DKMT_HANDLE), allocation->getHandles().data(), allocation->getNumHandles() * sizeof(D3DKMT_HANDLE));
            totalHandlesCount += allocation->getNumHandles();
        }
    }

    bool result = true;
    if (totalHandlesCount) {
        uint64_t bytesToTrim = 0;
        while ((result = wddm.makeResident(handlesForResidency.get(), totalHandlesCount, false, &bytesToTrim)) == false) {
            this->setMemoryBudgetExhausted();
            const bool trimmingDone = this->trimResidencyToBudget(bytesToTrim);
            if (!trimmingDone) {
                auto evictionStatus = wddm.getTemporaryResourcesContainer()->evictAllResources();
                if (evictionStatus == EvictionStatus::SUCCESS) {
                    continue;
                }
                DEBUG_BREAK_IF(evictionStatus != EvictionStatus::NOT_APPLIED);
                result = wddm.makeResident(handlesForResidency.get(), totalHandlesCount, true, &bytesToTrim);
                break;
            }
        }
    }

    if (result == true) {
        for (uint32_t i = 0; i < residencyCount; i++) {
            WddmAllocation *allocation = static_cast<WddmAllocation *>(allocationsForResidency[i]);
            // Update fence value not to early destroy / evict allocation
            const auto currentFence = this->getMonitoredFence().currentFenceValue;
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

void WddmResidencyController::makeNonResidentEvictionAllocations(ResidencyContainer &evictionAllocations) {
    auto lock = this->acquireLock();
    const size_t residencyCount = evictionAllocations.size();

    for (uint32_t i = 0; i < residencyCount; i++) {
        WddmAllocation *allocation = static_cast<WddmAllocation *>(evictionAllocations[i]);
        this->addToTrimCandidateList(allocation);
    }
}

} // namespace NEO
