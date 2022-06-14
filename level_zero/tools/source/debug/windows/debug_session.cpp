/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/windows/debug_session.h"

namespace L0 {

DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd);

DebugSessionWindows::~DebugSessionWindows() {
    closeAsyncThread();
}

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
    } else {
        debugSession->startAsyncThread();
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
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_ATTACH_DEBUGGER: Failed - ProcessId: %d Status: 0x%llX EscapeReturnStatus: %d\n", processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateNtStatusToZeResult(status);
    }

    if (DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_ATTACH_DEBUGGER: Failed - ProcessId: %d Status: 0x%llX EscapeReturnStatus: %d\n", processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }

    debugHandle = escapeInfo.KmEuDbgL0EscapeInfo.AttachDebuggerParams.hDebugHandle;
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_ATTACH_DEBUGGER: SUCCESS - ProcessId: %d DebugHandle: 0x%llx\n", processId, debugHandle);

    auto result = ZE_RESULT_SUCCESS;
    do {
        result = readAndHandleEvent(100);
    } while (result == ZE_RESULT_SUCCESS && !moduleDebugAreaCaptured);

    if (moduleDebugAreaCaptured) {
        return ZE_RESULT_SUCCESS;
    }

    return result;
}

bool DebugSessionWindows::closeConnection() {
    if (debugHandle == invalidHandle) {
        return false;
    }

    closeAsyncThread();

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_DETACH_DEBUGGER;
    escapeInfo.KmEuDbgL0EscapeInfo.DetachDebuggerParams.ProcessID = processId;

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_DETACH_DEBUGGER: Failed - ProcessId: %d Status: 0x%llX\n", processId, status);
        return false;
    }

    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_DETACH_DEBUGGER: SUCCESS\n");
    return true;
}

void DebugSessionWindows::startAsyncThread() {
    asyncThread.thread = NEO::Thread::create(asyncThreadFunction, reinterpret_cast<void *>(this));
}

void DebugSessionWindows::closeAsyncThread() {
    asyncThread.close();
}

void *DebugSessionWindows::asyncThreadFunction(void *arg) {
    DebugSessionWindows *self = reinterpret_cast<DebugSessionWindows *>(arg);
    PRINT_DEBUGGER_INFO_LOG("Debugger async thread start\n", "");

    while (self->asyncThread.threadActive) {
        self->readAndHandleEvent(100);
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread closing\n", "");

    self->asyncThread.threadFinished.store(true);
    return nullptr;
}

NTSTATUS DebugSessionWindows::runEscape(KM_ESCAPE_INFO &escapeInfo) {
    D3DKMT_ESCAPE escapeCommand = {0};

    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.Header.Size = sizeof(escapeInfo) - sizeof(escapeInfo.Header);
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

ze_result_t DebugSessionWindows::readAndHandleEvent(uint64_t timeoutMs) {
    KM_ESCAPE_INFO escapeInfo = {0};

    union {
        READ_EVENT_PARAMS_BUFFER eventParamsBuffer;
        uint8_t rawBytes[READ_EVENT_PARAMS_BUFFER_MIN_SIZE_BYTES] = {0};
    } eventParamsBuffer;

    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_READ_EVENT;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.TimeoutMs = timeoutMs;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.EventParamsBufferSize = sizeof(eventParamsBuffer);
    escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.EventParamBufferPtr = reinterpret_cast<uint64_t>(&eventParamsBuffer);

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_EVENT: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateNtStatusToZeResult(status);
    }

    if (DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_EVENT: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }

    auto seqNo = escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.EventSeqNo;
    switch (escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.ReadEventType) {
    case DBGUMD_READ_EVENT_MODULE_CREATE_NOTIFICATION:
        return handleModuleCreateEvent(eventParamsBuffer.eventParamsBuffer.ModuleCreateEventParams);
    case DBGUMD_READ_EVENT_EU_ATTN_BIT_SET:
        return handleEuAttentionBitsEvent(eventParamsBuffer.eventParamsBuffer.EuBitSetEventParams);
    case DBGUMD_READ_EVENT_ALLOCATION_DATA_INFO:
        return handleAllocationDataEvent(seqNo, eventParamsBuffer.eventParamsBuffer.ReadAdditionalAllocDataParams);
    case DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY:
        return handleContextCreateDestroyEvent(eventParamsBuffer.eventParamsBuffer.ContextCreateDestroyEventParams);
    case DBGUMD_READ_EVENT_DEVICE_CREATE_DESTROY:
        return handleDeviceCreateDestroyEvent(eventParamsBuffer.eventParamsBuffer.DeviceCreateDestroyEventParams);
    case DBGUMD_READ_EVENT_CREATE_DEBUG_DATA:
        return handleCreateDebugDataEvent(eventParamsBuffer.eventParamsBuffer.ReadCreateDebugDataParams);
    default:
        break;
    }

    PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_EVENT: Unknown ReadEventType returned: %d\n", escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.ReadEventType);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DebugSessionWindows::handleModuleCreateEvent(DBGUMD_READ_EVENT_MODULE_CREATE_EVENT_PARAMS &moduleCreateParams) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::handleEuAttentionBitsEvent(DBGUMD_READ_EVENT_EU_ATTN_BIT_SET_PARAMS &euAttentionBitsParams) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::handleAllocationDataEvent(uint32_t seqNo, DBGUMD_READ_EVENT_READ_ALLOCATION_DATA_PARAMS &allocationDataParams) {
    auto allocationDebugData = reinterpret_cast<GFX_ALLOCATION_DEBUG_DATA_INFO *>(&allocationDataParams.DebugDataBufferPtr);
    UNRECOVERABLE_IF(nullptr == allocationDebugData);

    for (uint32_t i = 0; i < allocationDataParams.NumOfDebugData; ++i) {
        auto dataType = allocationDebugData[i].DataType;
        PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_READ_ALLOCATION_DATA: DataType=%d DataSize=%d DataPointer=%p\n", dataType, allocationDebugData[i].DataSize, allocationDebugData[i].DataPointer);
        NEO::WddmAllocation::RegistrationData registrationData = {0};
        auto result = readAllocationDebugData(seqNo, allocationDebugData[i].DataPointer, &registrationData, sizeof(registrationData));
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_ALLOCATION_DATA - Fail!\n");
            return result;
        }

        if (allocationDebugData->DataType == MODULE_HEAP_DEBUG_AREA) {
            moduleDebugAreaCaptured = true;
        }
        PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_READ_ALLOCATION_DATA - Success - gpuVA=0x%llX Size=0x%X\n", registrationData.gpuVirtualAddress, registrationData.size);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::handleContextCreateDestroyEvent(DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY_EVENT_PARAMS &contextCreateDestroyParams) {
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY_EVENT_PARAMS: hContextHandle: 0x%ullx IsCreated: %d SIPInstalled: %d\n", contextCreateDestroyParams.hContextHandle, contextCreateDestroyParams.IsCreated, contextCreateDestroyParams.IsSIPInstalled);
    if (!contextCreateDestroyParams.IsSIPInstalled) {
        return ZE_RESULT_SUCCESS;
    }
    {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);
        if (contextCreateDestroyParams.IsCreated) {
            allContexts.insert(contextCreateDestroyParams.hContextHandle);
        } else {
            allContexts.erase(contextCreateDestroyParams.hContextHandle);
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::handleDeviceCreateDestroyEvent(DBGUMD_READ_EVENT_DEVICE_CREATE_DESTROY_EVENT_PARAMS &deviceCreateDestroyParams) {
    PRINT_DEBUGGER_INFO_LOG("DEVICE_CREATE_DESTROY_EVENT: hDeviceContext=0x%llx IsCreated=%d ProcessId=%d\n",
                            deviceCreateDestroyParams.hDeviceContext, deviceCreateDestroyParams.IsCreated, deviceCreateDestroyParams.ProcessID);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::readAllocationDebugData(uint32_t seqNo, uint64_t umdDataBufferPtr, void *outBuf, size_t outBufSize) {
    KM_ESCAPE_INFO escapeInfo = {0};

    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_READ_ALLOCATION_DATA;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadAdditionalAllocDataParams.EventSeqNo = seqNo;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadAdditionalAllocDataParams.DebugDataAddr = umdDataBufferPtr;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadAdditionalAllocDataParams.OutputBufferSize = static_cast<uint32_t>(outBufSize);
    escapeInfo.KmEuDbgL0EscapeInfo.ReadAdditionalAllocDataParams.OutputBufferPtr = reinterpret_cast<uint64_t>(outBuf);

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_ALLOCATION_DATA: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateNtStatusToZeResult(status);
    }

    if (DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_ALLOCATION_DATA: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::handleCreateDebugDataEvent(DBGUMD_READ_EVENT_CREATE_DEBUG_DATA_PARAMS &createDebugDataParams) {
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_READ_EVENT_CREATE_DEBUG_DATA_PARAMS:. Type: %d BufferPtr: 0x%ullx DataSize: 0x%ullx\n", createDebugDataParams.DebugDataType, createDebugDataParams.DataBufferPtr, createDebugDataParams.DataSize);
    std::unique_lock<std::mutex> lock(asyncThreadMutex);
    if (createDebugDataParams.DebugDataType == ELF_BINARY) {
        ElfRange elf;
        elf.startVA = createDebugDataParams.DataBufferPtr;
        elf.endVA = elf.startVA + createDebugDataParams.DataSize;
        allElfs.push_back(elf);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::translateNtStatusToZeResult(NTSTATUS status) {
    switch (status) {
    case STATUS_SUCCESS:
        return ZE_RESULT_SUCCESS;
    case STATUS_INVALID_PARAMETER:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    case STATUS_UNSUCCESSFUL:
        return ZE_RESULT_ERROR_UNKNOWN;
    default:
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t DebugSessionWindows::translateEscapeReturnStatusToZeResult(uint32_t escapeReturnStatus) {
    switch (escapeReturnStatus) {
    case DBGUMD_RETURN_ESCAPE_SUCCESS:
        return ZE_RESULT_SUCCESS;
    case DBGUMD_RETURN_DEBUGGER_ATTACH_DEVICE_BUSY:
    case DBGUMD_RETURN_READ_EVENT_TIMEOUT_EXPIRED:
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    case DBGUMD_RETURN_INVALID_ARGS:
    case DBGUMD_RETURN_NOT_VALID_PROCESS:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    case DBGUMD_RETURN_PERMISSION_DENIED:
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    case DBGUMD_RETURN_EU_DEBUG_NOT_SUPPORTED:
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    default:
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t DebugSessionWindows::readEvent(uint64_t timeout, zet_debug_event_t *event) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DebugSessionWindows::readElfSpace(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool DebugSessionWindows::isVAElf(const zet_debug_memory_space_desc_t *desc, size_t size) {
    std::unique_lock<std::mutex> lock(asyncThreadMutex);
    for (auto elf : allElfs) {
        if (desc->address >= elf.startVA && desc->address <= elf.endVA) {
            if (desc->address + size > elf.endVA) {
                return false;
            }
            return true;
        }
    }
    return false;
}

ze_result_t DebugSessionWindows::readMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {

    if (debugHandle == invalidHandle) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (!isValidGpuAddress(desc->address)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t status = ZE_RESULT_ERROR_UNINITIALIZED;
    status = sanityMemAccessThreadCheck(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (isVAElf(desc, size)) {
        return readElfSpace(desc, size, buffer);
    }

    uint64_t memoryHandle = DebugSessionWindows::invalidHandle;

    if (DebugSession::isThreadAll(thread)) {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);
        if (allContexts.empty()) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        memoryHandle = *allContexts.begin();
    } else {
        auto threadId = convertToThreadId(thread);
        memoryHandle = allThreads[threadId]->getMemoryHandle();
        if (memoryHandle == EuThread::invalidHandle) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }
    return readGpuMemory(memoryHandle, static_cast<char *>(buffer), size, desc->address);
}

ze_result_t DebugSessionWindows::writeMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {

    if (debugHandle == invalidHandle) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (!isValidGpuAddress(desc->address)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t status = ZE_RESULT_ERROR_UNINITIALIZED;
    status = sanityMemAccessThreadCheck(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint64_t memoryHandle = DebugSessionWindows::invalidHandle;

    if (DebugSession::isThreadAll(thread)) {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);
        if (allContexts.empty()) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        memoryHandle = *allContexts.begin();
    } else {
        auto threadId = convertToThreadId(thread);
        memoryHandle = allThreads[threadId]->getMemoryHandle();
        if (memoryHandle == EuThread::invalidHandle) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    }

    return writeGpuMemory(memoryHandle, static_cast<const char *>(buffer), size, desc->address);
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

    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    gpuVa = gmmHelper->decanonize(gpuVa);

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_READ_GFX_MEMORY;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.hContextHandle = memoryHandle;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.GpuVirtualAddr = gpuVa;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferSize = static_cast<uint32_t>(size);
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferPtr = reinterpret_cast<uint64_t>(output);

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status || DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_GFX_MEMORY: Failed - ProcessId: %d Status: %d EscapeReturnStatus: %d\n", processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::writeGpuMemory(uint64_t memoryHandle, const char *input, size_t size, uint64_t gpuVa) {

    auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
    gpuVa = gmmHelper->decanonize(gpuVa);

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_WRITE_GFX_MEMORY;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.hContextHandle = memoryHandle;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.GpuVirtualAddr = gpuVa;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferSize = static_cast<uint32_t>(size);
    escapeInfo.KmEuDbgL0EscapeInfo.ReadGfxMemoryParams.MemoryBufferPtr = reinterpret_cast<uint64_t>(input);

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status || DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_GFX_MEMORY: Failed - ProcessId: %d Status: %d EscapeReturnStatus: %d\n", processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }
    return ZE_RESULT_SUCCESS;
}

void DebugSessionWindows::enqueueApiEvent(zet_debug_event_t &debugEvent) {
}

bool DebugSessionWindows::readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srMagic) {
    return false;
}

bool DebugSessionWindows::readModuleDebugArea() {
    return false;
}

ze_result_t DebugSessionWindows::readSbaBuffer(EuThread::ThreadId, NEO::SbaTrackedAddresses &sbaBuffer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
