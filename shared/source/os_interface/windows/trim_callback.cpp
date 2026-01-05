/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"

namespace NEO {

VOID *Wddm::registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback) {
    if (!hwDeviceId || !this->rootDeviceEnvironment.executionEnvironment.osEnvironment || !getGdi()) {
        return nullptr;
    }

    if (debugManager.flags.DoNotRegisterTrimCallback.get()) {
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
    if (!hwDeviceId || !this->rootDeviceEnvironment.executionEnvironment.osEnvironment || !getGdi()) {
        return;
    }

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

    if (residencyController->wddm.isShutdownInProgress()) {
        return;
    }

    residencyController->trimResidency(trimNotification->Flags, trimNotification->NumBytesToTrim);
}

void WddmResidencyController::trimResidency(const D3DDDI_TRIMRESIDENCYSET_FLAGS &flags, uint64_t bytes) {
    std::chrono::high_resolution_clock::time_point callbackStart;
    perfLogResidencyTrimCallbackBegin(wddm.getResidencyLogger(), callbackStart);

    if (flags.PeriodicTrim) {
        uint64_t sizeToTrim = 0;
        auto lock = this->acquireLock();
        WddmAllocation *wddmAllocation = nullptr;
        std::vector<D3DKMT_HANDLE> handlesToEvict;
        std::vector<WddmAllocation *> allocationsToRemove;
        allocationsToRemove.reserve(evictionAllocations.size());
        handlesToEvict.reserve(evictionAllocations.size());

        this->wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
            for (auto allocationIter = evictionAllocations.begin(); allocationIter != evictionAllocations.end(); ++allocationIter) {
                auto &osContext = static_cast<NEO::OsContextWin &>(*engine.osContext);
                uint32_t osContextId = osContext.getContextId();

                wddmAllocation = reinterpret_cast<WddmAllocation *>(*allocationIter);

                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "lastPeriodicTrimFenceValue = ", osContext.getLastTrimFenceValue());

                if (osContext.wasAllocationUsedSinceLastTrim(wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId)) ||
                    wddmAllocation->isAlwaysResident(osContextId) ||
                    !wddmAllocation->getResidencyData().resident[osContextId]) {
                    continue;
                }

                if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                    for (auto i = 0u; i < wddmAllocation->getNumGmms(); i++) {
                        handlesToEvict.push_back(wddmAllocation->getHandles()[i]);
                    }
                } else {
                    for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                        AllocationStorageData &fragmentStorageData = wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId];
                        if (!osContext.wasAllocationUsedSinceLastTrim(fragmentStorageData.residency->getFenceValueForContextId(osContextId)) && fragmentStorageData.residency->resident[osContextId]) {
                            auto osHandle = static_cast<OsHandleWin *>(wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].osHandleStorage);
                            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict fragment: handle =", osHandle->handle, "lastFence =",
                                    wddmAllocation->fragmentsStorage.fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId));

                            handlesToEvict.push_back(static_cast<OsHandleWin *>(fragmentStorageData.osHandleStorage)->handle);
                            fragmentStorageData.residency->resident[osContextId] = false;
                        }
                    }
                }

                DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict allocation, gpu address = ", std::hex, wddmAllocation->getGpuAddress(), "lastFence =", wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId));
                wddmAllocation->getResidencyData().resident[osContextId] = false;

                allocationsToRemove.push_back(wddmAllocation);
            }

            this->wddm.evict(handlesToEvict.data(), static_cast<uint32_t>(handlesToEvict.size()), sizeToTrim, false);
            handlesToEvict.clear();
        });

        std::for_each(allocationsToRemove.begin(), allocationsToRemove.end(), [this](GraphicsAllocation *allocationToRemove) { removeAllocationImpl(evictionAllocations, allocationToRemove); });
    }

    if (flags.TrimToBudget) {
        StackVec<std::unique_lock<CommandStreamReceiver::MutexType>, 5> csrLocks;
        this->wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
            csrLocks.push_back(engine.commandStreamReceiver->obtainUniqueOwnership());
        });
        auto lock = this->acquireLock();
        trimResidencyToBudget(bytes);
    }

    if (flags.PeriodicTrim || flags.RestartPeriodicTrim) {
        this->wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
            auto osContext = static_cast<NEO::OsContextWin *>(engine.osContext);
            osContext->updateLastTrimFenceValue();
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "updated lastPeriodicTrimFenceValue =", osContext->getLastTrimFenceValue());
        });
    }

    perfLogResidencyTrimCallbackEnd(wddm.getResidencyLogger(), flags.Value, this, callbackStart);
}

bool WddmResidencyController::trimResidencyToBudget(uint64_t bytes) {
    uint64_t sizeToTrim = 0;
    uint64_t numberOfBytesToTrim = bytes;
    WddmAllocation *wddmAllocation = nullptr;
    perfLogResidencyTrimToBudget(wddm.getResidencyLogger(), bytes, this);

    auto allocationIter = evictionAllocations.begin();
    std::vector<D3DKMT_HANDLE> handlesToEvict;
    handlesToEvict.reserve(evictionAllocations.size());
    std::vector<WddmAllocation *> allocationsToRemove;
    allocationsToRemove.reserve(evictionAllocations.size());

    this->wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
        auto csr = engine.commandStreamReceiver;
        csr->drainPagingFenceQueue();
        auto &osContext = static_cast<NEO::OsContextWin &>(csr->getOsContext());
        uint32_t osContextId = osContext.getContextId();

        while (numberOfBytesToTrim > 0 && allocationIter != evictionAllocations.end()) {
            wddmAllocation = reinterpret_cast<WddmAllocation *>(*allocationIter);
            uint64_t lastFence = wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId);
            auto &monitoredFence = osContext.getMonitoredFence();

            if (lastFence > monitoredFence.lastSubmittedFence ||
                wddmAllocation->isAlwaysResident(osContextId) ||
                !wddmAllocation->getResidencyData().resident[osContextId]) {
                ++allocationIter;
                continue;
            }

            uint64_t sizeEvicted = 0;

            if (lastFence > *monitoredFence.cpuAddress) {
                this->wddm.waitFromCpu(lastFence, osContext.getMonitoredFence(), false);
            }

            if (wddmAllocation->fragmentsStorage.fragmentCount == 0) {
                for (auto i = 0u; i < wddmAllocation->getNumGmms(); i++) {
                    handlesToEvict.push_back(wddmAllocation->getHandles()[i]);
                }
                sizeEvicted = wddmAllocation->getAlignedSize();
            } else {
                auto &fragmentStorageData = wddmAllocation->fragmentsStorage.fragmentStorageData;
                for (uint32_t allocationId = 0; allocationId < wddmAllocation->fragmentsStorage.fragmentCount; allocationId++) {
                    if (fragmentStorageData[allocationId].residency->getFenceValueForContextId(osContextId) <= monitoredFence.lastSubmittedFence && fragmentStorageData[allocationId].residency->resident[osContextId]) {
                        handlesToEvict.push_back(static_cast<OsHandleWin *>(fragmentStorageData[allocationId].osHandleStorage)->handle);
                        fragmentStorageData[allocationId].residency->resident[osContextId] = false;
                        sizeEvicted += fragmentStorageData[allocationId].fragmentSize;
                    }
                }
            }
            DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Evict allocation, gpu address = ", std::hex, wddmAllocation->getGpuAddress(), "lastFence =", wddmAllocation->getResidencyData().getFenceValueForContextId(osContextId));

            if (sizeEvicted >= numberOfBytesToTrim) {
                numberOfBytesToTrim = 0;
            } else {
                numberOfBytesToTrim -= sizeEvicted;
            }

            wddmAllocation->getResidencyData().resident[osContextId] = false;
            allocationsToRemove.push_back(wddmAllocation);

            ++allocationIter;
        }

        this->wddm.evict(handlesToEvict.data(), static_cast<uint32_t>(handlesToEvict.size()), sizeToTrim, true);
        handlesToEvict.clear();
    });

    std::for_each(allocationsToRemove.begin(), allocationsToRemove.end(), [this](GraphicsAllocation *allocationToRemove) { removeAllocationImpl(evictionAllocations, allocationToRemove); });

    return numberOfBytesToTrim == 0;
}

} // namespace NEO