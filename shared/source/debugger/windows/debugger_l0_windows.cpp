/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_debug.h"

#include "KmEscape.h"

namespace NEO {
bool DebuggerL0::initDebuggingInOs(NEO::OSInterface *osInterface) {

    if (osInterface == nullptr) {
        return false;
    }
    if (osInterface->isDebugAttachAvailable() == false) {
        return false;
    }

    return true;
}

void DebuggerL0::initSbaTrackingMode() {
    singleAddressSpaceSbaTracking = true;
}

void DebuggerL0::registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) {
}

bool DebuggerL0::attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &kernelAlloc, uint32_t &moduleHandle) {
    return false;
}

bool DebuggerL0::removeZebinModule(uint32_t moduleHandle) {
    return false;
}

static NTSTATUS runEscape(NEO::Wddm *wddm, KM_ESCAPE_INFO &escapeInfo) {
    D3DKMT_ESCAPE escapeCommand = {0};

    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.Header.Size = sizeof(escapeInfo) - sizeof(escapeInfo.Header);
    escapeInfo.EscapeOperation = KM_ESCAPE_EUDBG_UMD_CREATE_DEBUG_DATA;

    escapeCommand.Flags.HardwareAccess = 0;
    escapeCommand.Flags.Reserved = 0;
    escapeCommand.hAdapter = wddm->getAdapter();
    escapeCommand.hContext = (D3DKMT_HANDLE)0;
    escapeCommand.hDevice = wddm->getDeviceHandle();
    escapeCommand.pPrivateDriverData = &escapeInfo;
    escapeCommand.PrivateDriverDataSize = sizeof(escapeInfo);
    escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

    return wddm->escape(escapeCommand);
}

void DebuggerL0::notifyCommandQueueCreated() {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);
        if (++commandQueueCount == 1) {
            auto pWddm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Wddm>();
            int val = 0;
            KM_ESCAPE_INFO escapeInfo = {0};
            escapeInfo.KmEuDbgUmdCreateDebugData.DataSize = sizeof(val);
            escapeInfo.KmEuDbgUmdCreateDebugData.DebugDataType = static_cast<uint32_t>(NEO::DebugDataType::CMD_QUEUE_CREATED);
            escapeInfo.KmEuDbgUmdCreateDebugData.hElfAddressPtr = reinterpret_cast<uint64_t>(&val);

            runEscape(pWddm, escapeInfo);
        }
    }
}

void DebuggerL0::notifyCommandQueueDestroyed() {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);
        if (--commandQueueCount == 0) {
            auto pWddm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Wddm>();
            int val = 0;
            KM_ESCAPE_INFO escapeInfo = {0};
            escapeInfo.KmEuDbgUmdCreateDebugData.DataSize = sizeof(val);
            escapeInfo.KmEuDbgUmdCreateDebugData.DebugDataType = static_cast<uint32_t>(NEO::DebugDataType::CMD_QUEUE_DESTROYED);
            escapeInfo.KmEuDbgUmdCreateDebugData.hElfAddressPtr = reinterpret_cast<uint64_t>(&val);

            runEscape(pWddm, escapeInfo);
        }
    }
}

} // namespace NEO
