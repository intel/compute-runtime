/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"

namespace NEO {

VOID *Wddm::registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController) {
    if (DebugManager.flags.DoNotRegisterTrimCallback.get()) {
        return nullptr;
    }
    D3DKMT_REGISTERTRIMNOTIFICATION registerTrimNotification;
    registerTrimNotification.Callback = reinterpret_cast<decltype(registerTrimNotification.Callback)>(callback);
    registerTrimNotification.AdapterLuid = hwDeviceId->getAdapterLuid();
    registerTrimNotification.Context = &residencyController;
    registerTrimNotification.hDevice = device;

    NTSTATUS status = getGdi()->registerTrimNotification(&registerTrimNotification);
    if (status == STATUS_SUCCESS) {
        return registerTrimNotification.Handle;
    }
    return nullptr;
}

void Wddm::unregisterTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, VOID *trimCallbackHandle) {
    DEBUG_BREAK_IF(callback == nullptr);
    if (trimCallbackHandle == nullptr || isShutdownInProgress()) {
        return;
    }
    D3DKMT_UNREGISTERTRIMNOTIFICATION unregisterTrimNotification;
    unregisterTrimNotification.Callback = callback;
    unregisterTrimNotification.Handle = trimCallbackHandle;

    [[maybe_unused]] NTSTATUS status = getGdi()->unregisterTrimNotification(&unregisterTrimNotification);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
}

void APIENTRY WddmResidencyController::trimCallback(_Inout_ D3DKMT_TRIMNOTIFICATION *trimNotification) {
    auto residencyController = static_cast<WddmResidencyController *>(trimNotification->Context);
    DEBUG_BREAK_IF(residencyController == nullptr);

    auto lock = residencyController->acquireTrimCallbackLock();
    residencyController->trimResidency(trimNotification->Flags, trimNotification->NumBytesToTrim);
}

void WddmResidencyController::trimResidency(const D3DDDI_TRIMRESIDENCYSET_FLAGS &flags, uint64_t bytes) {
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
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "allocation: default handle =", wddmAllocation->getDefaultHandle(), "lastFence =", wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId));

            uint32_t fragmentsToEvict = 0;

            if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict allocation: default handle =", wddmAllocation->getDefaultHandle(), "lastFence =", wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId));
                this->wddm.evict(&wddmAllocation->getHandles()[0], wddmAllocation->getNumGmms(), sizeToTrim, false);
            }

            for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                AllocationStorageData &fragmentStorageData = wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId];
                if (!wasAllocationUsedSinceLastTrim(fragmentStorageData.residency->getFenceValueForContextId(osContextId))) {
                    auto osHandle = static_cast<OsHandleWin *>(wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage);
                    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict fragment: handle =", osHandle->handle, "lastFence =",
                            wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId));
                    fragmentEvictHandles[fragmentsToEvict++] = static_cast<OsHandleWin *>(fragmentStorageData.osHandleStorage)->handle;
                    fragmentStorageData.residency->resident[osContextId] = false;
                }
            }
            if (fragmentsToEvict != 0) {
                this->wddm.evict((D3DKMT_HANDLE *)fragmentEvictHandles, fragmentsToEvict, sizeToTrim, false);
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
            this->wddm.evict(&wddmAllocation->getHandles()[0], wddmAllocation->getNumGmms(), sizeToTrim, true);
            sizeEvicted = wddmAllocation->getAlignedSize();
        } else {
            auto &fragmentStorageData = wddmAllocation->fragmentsStorage.fragmentStorageData;
            for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                if (fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId) <= monitoredFence.lastSubmittedFence) {
                    fragmentEvictHandles[fragmentsToEvict++] = static_cast<OsHandleWin *>(fragmentStorageData[allocationId].osHandleStorage)->handle;
                }
            }

            if (fragmentsToEvict != 0) {
                this->wddm.evict((D3DKMT_HANDLE *)fragmentEvictHandles, fragmentsToEvict, sizeToTrim, true);

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

} // namespace NEO