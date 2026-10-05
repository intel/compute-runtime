/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

NTSTATUS Wddm::createNTHandle(const D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle) {
    OBJECT_ATTRIBUTES objAttr = {};
    objAttr.Length = sizeof(OBJECT_ATTRIBUTES);

    return getGdi()->shareObjects(1, resourceHandle, &objAttr, SHARED_ALLOCATION_ALL_ACCESS, ntHandle);
}

bool Wddm::getReadOnlyFlagValue(const void *cpuPtr) const {
    return !isAligned<MemoryConstants::pageSize>(cpuPtr);
}
bool Wddm::isReadOnlyFlagFallbackSupported() const {
    return true;
}

HANDLE Wddm::createMonitoredFenceKmdWaitEvent() {
    return SysCalls::createEvent(nullptr, TRUE, FALSE, nullptr);
}

bool Wddm::resetMonitoredFenceKmdWaitEvent(HANDLE eventHandle) {
    return SysCalls::resetEvent(eventHandle);
}

bool Wddm::waitForMonitoredFenceKmdWaitEvent(HANDLE eventHandle, uint32_t timeoutMilliseconds) {
    return SysCalls::waitForSingleObject(eventHandle, timeoutMilliseconds) == WAIT_OBJECT_0;
}

class Wddm::MonitoredFenceKmdWaiter : public KmdWaiter {
  public:
    MonitoredFenceKmdWaiter(Wddm &wddm, const MonitoredFence &monitoredFence, uint64_t fenceValue, const KmdWaitHandles &handles)
        : wddm(wddm), monitoredFence(monitoredFence), fenceValue(fenceValue), handles(handles) {}

    ~MonitoredFenceKmdWaiter() override {
        if (this->osWaitFailed) {
            SysCalls::closeHandle(this->handles.timerHandle);
            SysCalls::closeHandle(this->handles.eventHandle);
            return;
        }
        std::lock_guard lock(this->wddm.unusedKmdWaitHandlesMutex);
        this->wddm.unusedKmdWaitHandles.push_back(this->handles);
    }

    std::optional<WaitStatus> wait(uint64_t timeoutNanoseconds) override {
        if (!this->isFenceReached() && !this->waitForEventOrTimer(timeoutNanoseconds)) {
            this->osWaitFailed = true;
            return std::nullopt;
        }
        if (*this->monitoredFence.cpuAddress == Wddm::gpuHangIndication) {
            return WaitStatus::gpuHang;
        }
        return this->isFenceReached() ? WaitStatus::ready : WaitStatus::notReady;
    }

  protected:
    bool isFenceReached() const { return this->fenceValue <= *this->monitoredFence.cpuAddress; }

    bool waitForEventOrTimer(uint64_t timeoutNanoseconds) {
        constexpr uint64_t nanosecondsPerTimerUnit = 100;
        LARGE_INTEGER relativeDueTime = {};
        relativeDueTime.QuadPart = -static_cast<LONGLONG>(timeoutNanoseconds / nanosecondsPerTimerUnit);
        if (!SysCalls::setWaitableTimer(this->handles.timerHandle, &relativeDueTime, 0, nullptr, nullptr, FALSE)) {
            return false;
        }
        const HANDLE waitHandles[] = {this->handles.eventHandle, this->handles.timerHandle};
        const auto signaledHandleIndex = SysCalls::waitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (signaledHandleIndex == WAIT_OBJECT_0) {
            // A reused event can be signaled by a wait registered by its previous owner.
            return this->isFenceReached() || SysCalls::resetEvent(this->handles.eventHandle);
        }
        return signaledHandleIndex == WAIT_OBJECT_0 + 1;
    }

    Wddm &wddm;
    const MonitoredFence &monitoredFence;
    const uint64_t fenceValue;
    const KmdWaitHandles handles;
    bool osWaitFailed = false;
};

std::unique_ptr<KmdWaiter> Wddm::createMonitoredFenceKmdWaiter(const MonitoredFence &monitoredFence, uint64_t fenceValue) {
    KmdWaitHandles handles = {};
    {
        std::lock_guard lock(this->unusedKmdWaitHandlesMutex);
        if (!this->unusedKmdWaitHandles.empty()) {
            handles = this->unusedKmdWaitHandles.back();
            this->unusedKmdWaitHandles.pop_back();
        }
    }
    if (handles.eventHandle == nullptr) {
        handles.eventHandle = SysCalls::createEvent(nullptr, TRUE, FALSE, nullptr);
        handles.timerHandle = SysCalls::createWaitableTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
    }
    if ((handles.eventHandle == nullptr) || (handles.timerHandle == nullptr) || !SysCalls::resetEvent(handles.eventHandle)) {
        for (const auto handle : {handles.eventHandle, handles.timerHandle}) {
            if (handle != nullptr) {
                SysCalls::closeHandle(handle);
            }
        }
        return nullptr;
    }

    auto kmdWaiter = std::make_unique<MonitoredFenceKmdWaiter>(*this, monitoredFence, fenceValue, handles);
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU waitFromCpu = {};
    waitFromCpu.ObjectCount = 1;
    waitFromCpu.ObjectHandleArray = &monitoredFence.fenceHandle;
    waitFromCpu.FenceValueArray = &fenceValue;
    waitFromCpu.hDevice = this->device;
    waitFromCpu.hAsyncEvent = handles.eventHandle;
    if (this->getGdi()->waitForSynchronizationObjectFromCpu(&waitFromCpu) != STATUS_SUCCESS) {
        return nullptr;
    }
    return kmdWaiter;
}

void Wddm::releaseUnusedKmdWaitHandles() {
    for (const auto &handles : this->unusedKmdWaitHandles) {
        SysCalls::closeHandle(handles.timerHandle);
        SysCalls::closeHandle(handles.eventHandle);
    }
    this->unusedKmdWaitHandles.clear();
}

HANDLE Wddm::getSharedHandle(const MemoryManager::OsHandleData &osHandleData) {
    HANDLE sharedNtHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(osHandleData.handle));
    if (osHandleData.parentProcessId != 0) {
        // Open the parent process handle with required access rights
        HANDLE parentProcessHandle = NEO::SysCalls::openProcess(PROCESS_DUP_HANDLE, FALSE, static_cast<DWORD>(osHandleData.parentProcessId));
        if (parentProcessHandle == nullptr) {
            DEBUG_BREAK_IF(true);
            return sharedNtHandle;
        }

        // Duplicate the handle from the parent process to the current process
        // This is necessary to ensure that the handle can be used in the current process context
        // We use GENERIC_READ | GENERIC_WRITE to ensure we can perform operations on the handle
        HANDLE duplicatedHandle = nullptr;
        BOOL duplicateResult = NEO::SysCalls::duplicateHandle(
            parentProcessHandle,
            reinterpret_cast<HANDLE>(static_cast<uintptr_t>(osHandleData.handle)),
            GetCurrentProcess(),
            &duplicatedHandle,
            GENERIC_READ | GENERIC_WRITE,
            FALSE,
            0);

        // Close the parent process handle as we no longer need it
        // The duplicated handle will be used for further operations
        NEO::SysCalls::closeHandle(parentProcessHandle);

        if (!duplicateResult) {
            DEBUG_BREAK_IF(true);
            return sharedNtHandle;
        }
        sharedNtHandle = duplicatedHandle;
    }
    return sharedNtHandle;
}

bool Wddm::isLatePreemptionStartSupported(const HardwareInfo &hwInfo) {
    if (debugManager.flags.OverrideLatePreemptionStart.get() != -1) {
        return debugManager.flags.OverrideLatePreemptionStart.get();
    }
    return hwInfo.featureTable.flags.ftrSelectiveWmtp && hwInfo.caps.latePreemptionStartSupported;
}

void OsContextWin::prepareLatePreemptionStart(CREATECONTEXT_PVTDATA &privateData) {
    PRINT_STRING(debugManager.flags.PrintLateMidThreadPreemptionStartInfo.get(), stdout, "Late Mid Thread Preemption Start: Prepare private context data\n");

    UNRECOVERABLE_IF(NULL != latePreemptionStartEventHandle);
    latePreemptionStartEventHandle = SysCalls::createEvent(NULL, TRUE, FALSE, NULL);
    UNRECOVERABLE_IF(NULL == latePreemptionStartEventHandle);

    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 syncObjectInfo = {};
    syncObjectInfo.hDevice = wddm.getDeviceHandle();
    syncObjectInfo.Info.Type = D3DDDI_CPU_NOTIFICATION;
    syncObjectInfo.Info.CPUNotification.Event = latePreemptionStartEventHandle;
    syncObjectInfo.Info.Flags.SignalByKmd = TRUE;
    auto status = wddm.getGdi()->createSynchronizationObject2(&syncObjectInfo);
    UNRECOVERABLE_IF(STATUS_SUCCESS != status);
    latePreemptionStartSyncObjectHandle = syncObjectInfo.hSyncObject;

    D3DDDI_DRIVERESCAPE_CPUEVENTUSAGE escapePrivateData = {};
    escapePrivateData.EscapeType = D3DDDI_DRIVERESCAPETYPE_CPUEVENTUSAGE;
    escapePrivateData.hSyncObject = latePreemptionStartSyncObjectHandle;
    escapePrivateData.Usage[0] = CPU_EVENT_USAGE_TYPE_SELECTIVE_PREEMPT;

    D3DKMT_ESCAPE escapeArguments = {};
    escapeArguments.hAdapter = wddm.getAdapter();
    escapeArguments.hDevice = wddm.getDeviceHandle();
    escapeArguments.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    escapeArguments.Flags.DriverKnownEscape = 1;
    escapeArguments.Flags.NoAdapterSynchronization = 1;
    escapeArguments.pPrivateDriverData = &escapePrivateData;
    escapeArguments.PrivateDriverDataSize = sizeof(escapePrivateData);

    status = wddm.escape(escapeArguments);
    UNRECOVERABLE_IF(STATUS_SUCCESS != status);

    auto callback = [](PVOID context, BOOLEAN b) {
        PRINT_STRING(debugManager.flags.PrintLateMidThreadPreemptionStartInfo.get(), stdout, "Late Mid Thread Preemption Start: Event signal received - enabling mid thread preemption\n");
        auto commandStreamReceiver = reinterpret_cast<CommandStreamReceiver *>(context);
        commandStreamReceiver->submitLateMidThreadPreemptionStart();
    };

    PRINT_STRING(debugManager.flags.PrintLateMidThreadPreemptionStartInfo.get(), stdout, "Late Mid Thread Preemption Start: Register wait\n");
    auto commandStreamReceiver = getCommandStreamReceiver();
    UNRECOVERABLE_IF(!commandStreamReceiver);
    auto result = SysCalls::registerWaitForSingleObject(&latePreemptionStartWaitObjectHandle, latePreemptionStartEventHandle, callback, commandStreamReceiver, INFINITE, WT_EXECUTELONGFUNCTION | WT_EXECUTEONLYONCE);
    UNRECOVERABLE_IF(result == FALSE);

    privateData.DisableWmtp = TRUE;
    privateData.NotifyPreemptExceedThreshold = TRUE;
    privateData.hPreemptCpuEventObject = escapePrivateData.hKmdCpuEvent;
}

void OsContextWin::stopLatePreemptionStartWait() {
    if (latePreemptionStartWaitObjectHandle) {
        PRINT_STRING(debugManager.flags.PrintLateMidThreadPreemptionStartInfo.get(), stdout, "Late Mid Thread Preemption Start: Unregister wait\n");
        auto result = SysCalls::unregisterWait(latePreemptionStartWaitObjectHandle);
        UNRECOVERABLE_IF(result == FALSE);
        latePreemptionStartWaitObjectHandle = NULL;
    }
    if (latePreemptionStartSyncObjectHandle) {
        D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySyncInfo = {.hSyncObject = latePreemptionStartSyncObjectHandle};
        auto status = wddm.getGdi()->destroySynchronizationObject(&destroySyncInfo);
        UNRECOVERABLE_IF(status != STATUS_SUCCESS);
        latePreemptionStartSyncObjectHandle = NULL;
    }
    if (latePreemptionStartEventHandle) {
        PRINT_STRING(debugManager.flags.PrintLateMidThreadPreemptionStartInfo.get(), stdout, "Late Mid Thread Preemption Start: Close event handle\n");
        auto result = SysCalls::closeHandle(latePreemptionStartEventHandle);
        UNRECOVERABLE_IF(result == FALSE);
        latePreemptionStartEventHandle = NULL;
    }
}

} // namespace NEO
