/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/windows/debug_session.h"

namespace L0 {

DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd);

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device, ze_result_t &result) {
    if (!device->getOsInterface().isDebugAttachAvailable()) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }

    auto debugSession = createDebugSessionHelper(config, device, 0);
    result = debugSession->initialize();
    if (result != ZE_RESULT_SUCCESS) {
        debugSession->closeConnection();
        delete debugSession;
        debugSession = nullptr;
    }

    return debugSession;
}

ze_result_t DebugSessionWindows::initialize() {
    wddm = connectedDevice->getOsInterface().getDriverModel()->as<NEO::Wddm>();
    UNRECOVERABLE_IF(wddm == nullptr);

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_ATTACH_DEBUGGER;
    escapeInfo.KmEuDbgL0EscapeInfo.AttachDebuggerParams.hAdapter = wddm->getAdapter();
    escapeInfo.KmEuDbgL0EscapeInfo.AttachDebuggerParams.ProcessId = processId;

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_ATTACH_DEBUGGER: Failed - ProcessId: %d Status: %d EscapeReturnStatus: %d\n", processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }

    debugHandle = escapeInfo.KmEuDbgL0EscapeInfo.AttachDebuggerParams.hDebugHandle;
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_ATTACH_DEBUGGER: SUCCESS - ProcessId: %d DebugHandle: 0x%ullx\n", processId, debugHandle);
    return ZE_RESULT_SUCCESS;
}

bool DebugSessionWindows::closeConnection() {
    if (debugHandle == 0) {
        return false;
    }

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_DETACH_DEBUGGER;
    escapeInfo.KmEuDbgL0EscapeInfo.DetachDebuggerParams.ProcessID = processId;

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_DETACH_DEBUGGER: Failed - ProcessId: %d Status: %d\n", processId, status);
        return false;
    }

    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_DETACH_DEBUGGER: SUCCESS\n");
    return true;
}

NTSTATUS DebugSessionWindows::runEscape(KM_ESCAPE_INFO &escapeInfo) {
    D3DKMT_ESCAPE escapeCommand = {0};

    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.EscapeOperation = KM_ESCAPE_EUDBG_L0_DBGUMD_HANDLER;
    escapeInfo.KmEuDbgL0EscapeInfo.hDebugHandle = debugHandle;

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

ze_result_t DebugSessionWindows::translateEscapeReturnStatusToZeResult(uint32_t escapeReturnStatus) {
    switch (escapeReturnStatus) {
    case DBGUMD_ACTION_ESCAPE_SUCCESS:
        return ZE_RESULT_SUCCESS;
    case DBGUMD_ACTION_DEBUGGER_ATTACH_DEVICE_BUSY:
    case DBGUMD_ACTION_READ_EVENT_TIMEOUT_EXPIRED:
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    case DBGUMD_ACTION_INVALID_ARGS:
    case DBGUMD_ACTION_NOT_VALID_PROCESS:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    case DBGUMD_ACTION_PERMISSION_DENIED:
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    case DBGUMD_ACTION_EU_DEBUG_NOT_SUPPORTED:
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    default:
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t DebugSessionWindows::readEvent(uint64_t timeout, zet_debug_event_t *event) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::acknowledgeEvent(const zet_debug_event_t *event) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::resumeImp(std::vector<ze_device_thread_t> threads, uint32_t deviceIndex) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::interruptImp(uint32_t deviceIndex) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::readGpuMemory(uint64_t memoryHandle, char *output, size_t size, uint64_t gpuVa) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void DebugSessionWindows::enqueueApiEvent(zet_debug_event_t &debugEvent) {
}

bool DebugSessionWindows::readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) {
    return false;
}

bool DebugSessionWindows::readModuleDebugArea() {
    return false;
}

void DebugSessionWindows::startAsyncThread() {
}

ze_result_t DebugSessionWindows::readSbaBuffer(EuThread::ThreadId, SbaTrackedAddresses &sbaBuffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
