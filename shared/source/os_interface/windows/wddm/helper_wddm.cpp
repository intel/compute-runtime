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
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

NTSTATUS Wddm::createNTHandle(const D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle) {
    OBJECT_ATTRIBUTES objAttr = {};
    objAttr.Length = sizeof(OBJECT_ATTRIBUTES);

    return getGdi()->shareObjects(1, resourceHandle, &objAttr, SHARED_ALLOCATION_WRITE, ntHandle);
}

bool Wddm::getReadOnlyFlagValue(const void *cpuPtr) const {
    return !isAligned<MemoryConstants::pageSize>(cpuPtr);
}
bool Wddm::isReadOnlyFlagFallbackSupported() const {
    return true;
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
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    return hwInfo.featureTable.flags.ftrSelectiveWmtp && releaseHelper->isLatePreemptionStartSupportedHelper();
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
