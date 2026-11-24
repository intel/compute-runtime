/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/preprocessor.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_residency_controller.h"
#include "shared/source/utilities/logger.h"

#include <algorithm>

namespace NEO {

namespace {
struct TrimContext {
    OsContextWin *osContext;
    CommandStreamReceiver *csr;
    bool csrSkipped;
};
using TrimContexts = StackVec<TrimContext, 32>;

TrimContexts getTrimContexts(Wddm &wddm, const StackVec<CommandStreamReceiver *, 5> &skippedCsrs) {
    TrimContexts trimContexts;
    wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
        const bool csrSkipped = std::find(skippedCsrs.begin(), skippedCsrs.end(), engine.commandStreamReceiver) != skippedCsrs.end();
        trimContexts.push_back({static_cast<OsContextWin *>(engine.osContext), engine.commandStreamReceiver, csrSkipped});
    });
    return trimContexts;
}

bool isAlwaysResidentInAnyContext(const GraphicsAllocation &allocation, const TrimContexts &trimContexts) {
    return std::any_of(trimContexts.begin(), trimContexts.end(), [&](const TrimContext &trimContext) {
        return allocation.isAlwaysResident(trimContext.osContext->getContextId());
    });
}

bool wasUsedSinceLastTrim(ResidencyData &residencyData, const TrimContexts &trimContexts) {
    return std::any_of(trimContexts.begin(), trimContexts.end(), [&](const TrimContext &trimContext) {
        return trimContext.osContext->wasAllocationUsedSinceLastTrim(residencyData.getFenceValueForContextId(trimContext.osContext->getContextId()));
    });
}

bool canWaitForLastUsage(ResidencyData &residencyData, const TrimContexts &trimContexts) {
    return std::all_of(trimContexts.begin(), trimContexts.end(), [&](const TrimContext &trimContext) {
        const auto lastFence = residencyData.getFenceValueForContextId(trimContext.osContext->getContextId());
        const auto &monitoredFence = trimContext.osContext->getMonitoredFence();
        return lastFence <= monitoredFence.lastSubmittedFence && !(trimContext.csrSkipped && lastFence > *monitoredFence.cpuAddress);
    });
}

void waitForLastUsage(Wddm &wddm, ResidencyData &residencyData, const TrimContexts &trimContexts) {
    for (const auto &trimContext : trimContexts) {
        const auto lastFence = residencyData.getFenceValueForContextId(trimContext.osContext->getContextId());
        const auto &monitoredFence = trimContext.osContext->getMonitoredFence();
        if (lastFence > *monitoredFence.cpuAddress) {
            wddm.waitFromCpu(lastFence, monitoredFence, false);
        }
    }
}
} // namespace

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
        std::vector<D3DKMT_HANDLE> handlesToEvict;
        std::vector<WddmAllocation *> allocationsToRemove;
        allocationsToRemove.reserve(evictionAllocations.size());
        handlesToEvict.reserve(evictionAllocations.size());
        const auto trimContexts = getTrimContexts(this->wddm, {});

        for (auto allocation : evictionAllocations) {
            auto wddmAllocation = static_cast<WddmAllocation *>(allocation);
            auto &residencyData = wddmAllocation->getResidencyData();

            if (trimContexts.empty() || !residencyData.resident || isAlwaysResidentInAnyContext(*wddmAllocation, trimContexts) || wasUsedSinceLastTrim(residencyData, trimContexts)) {
                continue;
            }

            if (wddmAllocation->getFragmentCount() == 0) {
                for (auto i = 0u; i < wddmAllocation->getNumGmms(); i++) {
                    handlesToEvict.push_back(wddmAllocation->getHandles()[i]);
                }
            } else {
                for (uint32_t allocationId = 0; allocationId < wddmAllocation->getFragmentCount(); allocationId++) {
                    AllocationStorageData &fragmentStorageData = wddmAllocation->fragmentsStorage->fragmentStorageData[allocationId];
                    if (fragmentStorageData.residency->resident && !wasUsedSinceLastTrim(*fragmentStorageData.residency, trimContexts)) {
                        auto osHandle = static_cast<OsHandleWin *>(fragmentStorageData.osHandleStorage);
                        DBG_LOG(ResidencyDebugEnable, "Residency:", NEO_FUNCTION_NAME, "Evict fragment: handle =", osHandle->handle);

                        handlesToEvict.push_back(osHandle->handle);
                        fragmentStorageData.residency->resident = false;
                    }
                }
            }

            DBG_LOG(ResidencyDebugEnable, "Residency:", NEO_FUNCTION_NAME, "Evict allocation, gpu address = ", std::hex, wddmAllocation->getGpuAddress());
            residencyData.resident = false;

            allocationsToRemove.push_back(wddmAllocation);
        }

        this->wddm.evict(handlesToEvict.data(), static_cast<uint32_t>(handlesToEvict.size()), sizeToTrim, false);

        std::for_each(allocationsToRemove.begin(), allocationsToRemove.end(), [this](GraphicsAllocation *allocationToRemove) { removeFromEvictionAllocations(allocationToRemove); });
    }

    if (flags.TrimToBudget) {
        StackVec<std::unique_lock<CommandStreamReceiver::MutexType>, 5> csrLocks;
        auto lockPrimaryCsrs = [&](bool bcs) {
            this->wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
                if (NEO::EngineHelpers::isBcs(engine.getEngineType()) == bcs && engine.commandStreamReceiver->getPrimaryCsr() == nullptr) {
                    csrLocks.push_back(engine.commandStreamReceiver->obtainUniqueOwnership());
                }
            });
        };
        lockPrimaryCsrs(true);
        lockPrimaryCsrs(false);
        auto lock = this->acquireLock();

        // Submission on a secondary CSR may wait for its primary CSR or another CSR, so never wait while holding or for a secondary CSR; skip busy ones.
        StackVec<CommandStreamReceiver *, 5> skippedCsrs;
        this->wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
            if (engine.commandStreamReceiver->getPrimaryCsr() == nullptr) {
                return;
            }
            auto csrLock = engine.commandStreamReceiver->tryObtainUniqueOwnership();
            if (csrLock.owns_lock()) {
                csrLocks.push_back(std::move(csrLock));
            } else {
                skippedCsrs.push_back(engine.commandStreamReceiver);
            }
        });
        trimResidencyToBudget(bytes, skippedCsrs);
    }

    if (flags.PeriodicTrim || flags.RestartPeriodicTrim) {
        this->wddm.forEachContextWithinWddm<true>([&](const EngineControl &engine) {
            auto osContext = static_cast<NEO::OsContextWin *>(engine.osContext);
            osContext->updateLastTrimFenceValue();
            DBG_LOG(ResidencyDebugEnable, "Residency:", NEO_FUNCTION_NAME, "updated lastPeriodicTrimFenceValue =", osContext->getLastTrimFenceValue());
        });
    }

    perfLogResidencyTrimCallbackEnd(wddm.getResidencyLogger(), flags.Value, this, callbackStart);
}

bool WddmResidencyController::trimResidencyToBudget(uint64_t bytes) {
    return trimResidencyToBudget(bytes, {});
}

bool WddmResidencyController::trimResidencyToBudget(uint64_t bytes, const StackVec<CommandStreamReceiver *, 5> &skippedCsrs) {
    uint64_t sizeToTrim = 0;
    uint64_t numberOfBytesToTrim = bytes;
    perfLogResidencyTrimToBudget(wddm.getResidencyLogger(), bytes, this);

    std::vector<D3DKMT_HANDLE> handlesToEvict;
    handlesToEvict.reserve(evictionAllocations.size());
    std::vector<WddmAllocation *> allocationsToRemove;
    allocationsToRemove.reserve(evictionAllocations.size());

    const auto trimContexts = getTrimContexts(this->wddm, skippedCsrs);
    if (trimContexts.empty()) {
        return numberOfBytesToTrim == 0;
    }
    for (const auto &trimContext : trimContexts) {
        if (!trimContext.csrSkipped) {
            trimContext.csr->drainPagingFenceQueue();
        }
    }

    for (auto allocationIter = evictionAllocations.begin(); numberOfBytesToTrim > 0 && allocationIter != evictionAllocations.end(); ++allocationIter) {
        auto wddmAllocation = static_cast<WddmAllocation *>(*allocationIter);
        auto &residencyData = wddmAllocation->getResidencyData();

        if (!residencyData.resident || isAlwaysResidentInAnyContext(*wddmAllocation, trimContexts) || !canWaitForLastUsage(residencyData, trimContexts)) {
            continue;
        }

        waitForLastUsage(this->wddm, residencyData, trimContexts);

        uint64_t sizeEvicted = 0;
        if (wddmAllocation->getFragmentCount() == 0) {
            for (auto i = 0u; i < wddmAllocation->getNumGmms(); i++) {
                handlesToEvict.push_back(wddmAllocation->getHandles()[i]);
            }
            sizeEvicted = wddmAllocation->getAlignedSize();
        } else {
            auto &fragmentStorageData = wddmAllocation->fragmentsStorage->fragmentStorageData;
            for (uint32_t allocationId = 0; allocationId < wddmAllocation->getFragmentCount(); allocationId++) {
                auto &fragmentResidency = *fragmentStorageData[allocationId].residency;
                if (fragmentResidency.resident && canWaitForLastUsage(fragmentResidency, trimContexts)) {
                    waitForLastUsage(this->wddm, fragmentResidency, trimContexts);
                    handlesToEvict.push_back(static_cast<OsHandleWin *>(fragmentStorageData[allocationId].osHandleStorage)->handle);
                    fragmentResidency.resident = false;
                    sizeEvicted += fragmentStorageData[allocationId].fragmentSize;
                }
            }
        }
        DBG_LOG(ResidencyDebugEnable, "Residency:", NEO_FUNCTION_NAME, "Evict allocation, gpu address = ", std::hex, wddmAllocation->getGpuAddress());

        numberOfBytesToTrim -= std::min(sizeEvicted, numberOfBytesToTrim);

        residencyData.resident = false;
        allocationsToRemove.push_back(wddmAllocation);
    }

    this->wddm.evict(handlesToEvict.data(), static_cast<uint32_t>(handlesToEvict.size()), sizeToTrim, true);

    std::for_each(allocationsToRemove.begin(), allocationsToRemove.end(), [this](GraphicsAllocation *allocationToRemove) { removeFromEvictionAllocations(allocationToRemove); });

    return numberOfBytesToTrim == 0;
}

} // namespace NEO
