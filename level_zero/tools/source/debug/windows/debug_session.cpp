/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/debug/windows/debug_session.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/windows/wddm_debug.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

#include "common/StateSaveAreaHeader.h"

namespace L0 {

DebugSession *createDebugSessionHelper(const zet_debug_config_t &config, Device *device, int debugFd, void *params);

DebugSessionWindows::~DebugSessionWindows() {
    closeAsyncThread();
}

DebugSession *DebugSession::create(const zet_debug_config_t &config, Device *device, ze_result_t &result, bool isRootAttach) {
    if (!device->getOsInterface().isDebugAttachAvailable() || !isRootAttach) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return nullptr;
    }

    auto debugSession = createDebugSessionHelper(config, device, 0, nullptr);
    debugSession->setAttachMode(isRootAttach);
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
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_ATTACH_DEBUGGER: SUCCESS - ProcessId: %d DebugHandle: 0x%llX\n", processId, debugHandle);

    auto result = ZE_RESULT_SUCCESS;

    do {
        result = readAndHandleEvent(100);
    } while (result == ZE_RESULT_SUCCESS && !moduleDebugAreaCaptured);

    if (moduleDebugAreaCaptured) {
        readModuleDebugArea();
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
        self->sendInterrupts();
        self->generateEventsAndResumeStoppedThreads();
    }

    PRINT_DEBUGGER_INFO_LOG("Debugger async thread closing\n", "");

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
        uint8_t rawBytes[2 * READ_EVENT_PARAMS_BUFFER_MIN_SIZE_BYTES] = {0};
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
        if (DBGUMD_RETURN_READ_EVENT_TIMEOUT_EXPIRED != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
            PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_EVENT: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        }
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }

    auto seqNo = escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.EventSeqNo;
    switch (escapeInfo.KmEuDbgL0EscapeInfo.ReadEventParams.ReadEventType) {
    case DBGUMD_READ_EVENT_MODULE_CREATE_NOTIFICATION:
        return handleModuleCreateEvent(seqNo, eventParamsBuffer.eventParamsBuffer.ModuleCreateEventParams);
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

ze_result_t DebugSessionWindows::handleModuleCreateEvent(uint32_t seqNo, DBGUMD_READ_EVENT_MODULE_CREATE_EVENT_PARAMS &moduleCreateParams) {
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_READ_EVENT_MODULE_CREATE_EVENT_PARAMS: hElfAddressPtr=0x%llX IsModuleCreate=%d LoadAddress=0x%llX ElfModuleSize=%d\n",
                            moduleCreateParams.hElfAddressPtr, moduleCreateParams.IsModuleCreate, moduleCreateParams.LoadAddress, moduleCreateParams.ElfModulesize);

    {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);
        if (moduleCreateParams.IsModuleCreate) {
            Module module = {0};
            module.cpuAddress = moduleCreateParams.hElfAddressPtr;
            module.gpuAddress = moduleCreateParams.LoadAddress;
            module.size = moduleCreateParams.ElfModulesize;
            allModules.push_back(module);
        } else {
            auto it = std::find_if(allModules.begin(), allModules.end(), [&](auto &m) { return m.gpuAddress == moduleCreateParams.LoadAddress; });
            if (it != allModules.end()) {
                moduleCreateParams.hElfAddressPtr = it->cpuAddress;
                moduleCreateParams.ElfModulesize = it->size;
                allModules.erase(it);
            }
        }
    }

    zet_debug_event_t debugEvent = {};
    debugEvent.type = moduleCreateParams.IsModuleCreate ? ZET_DEBUG_EVENT_TYPE_MODULE_LOAD : ZET_DEBUG_EVENT_TYPE_MODULE_UNLOAD;
    debugEvent.flags = moduleCreateParams.IsModuleCreate ? ZET_DEBUG_EVENT_FLAG_NEED_ACK : 0;
    debugEvent.info.module.format = ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF;
    debugEvent.info.module.load = moduleCreateParams.LoadAddress;
    debugEvent.info.module.moduleBegin = moduleCreateParams.hElfAddressPtr;
    debugEvent.info.module.moduleEnd = moduleCreateParams.hElfAddressPtr + moduleCreateParams.ElfModulesize;
    pushApiEvent(debugEvent, seqNo, DBGUMD_READ_EVENT_MODULE_CREATE_NOTIFICATION);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::handleEuAttentionBitsEvent(DBGUMD_READ_EVENT_EU_ATTN_BIT_SET_PARAMS &euAttentionBitsParams) {
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_READ_EVENT_EU_ATTN_BIT_SET_PARAMS: hContextHandle=0x%llX LRCA=%d BitMaskSizeInBytes=%d BitmaskArrayPtr=0x%llX\n",
                            euAttentionBitsParams.hContextHandle, euAttentionBitsParams.LRCA,
                            euAttentionBitsParams.BitMaskSizeInBytes, &euAttentionBitsParams.BitmaskArrayPtr);

    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    auto threadsWithAttention = l0GfxCoreHelper.getThreadsFromAttentionBitmask(hwInfo, 0u,
                                                                               reinterpret_cast<uint8_t *>(&euAttentionBitsParams.BitmaskArrayPtr),
                                                                               euAttentionBitsParams.BitMaskSizeInBytes);

    printBitmask(reinterpret_cast<uint8_t *>(&euAttentionBitsParams.BitmaskArrayPtr), euAttentionBitsParams.BitMaskSizeInBytes);

    PRINT_DEBUGGER_THREAD_LOG("ATTENTION received for thread count = %d\n", (int)threadsWithAttention.size());

    uint64_t memoryHandle = DebugSessionWindows::invalidHandle;
    {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);
        if (allContexts.empty()) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        memoryHandle = *allContexts.begin();
    }

    for (auto &threadId : threadsWithAttention) {
        PRINT_DEBUGGER_THREAD_LOG("ATTENTION event for thread: %s\n", EuThread::toString(threadId).c_str());
        markPendingInterruptsOrAddToNewlyStoppedFromRaisedAttention(threadId, memoryHandle);
    }

    checkTriggerEventsForAttention();

    return ZE_RESULT_SUCCESS;
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
            DEBUG_BREAK_IF(moduleDebugAreaCaptured && (registrationData.gpuVirtualAddress != this->debugAreaVA));
            moduleDebugAreaCaptured = true;
            this->debugAreaVA = registrationData.gpuVirtualAddress;
        } else if (allocationDebugData->DataType == SIP_CONTEXT_SAVE_AREA) {
            DEBUG_BREAK_IF(stateSaveAreaCaptured && (registrationData.gpuVirtualAddress != this->stateSaveAreaVA.load()));
            stateSaveAreaVA.store(registrationData.gpuVirtualAddress);
            stateSaveAreaCaptured = true;
        }

        PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_READ_ALLOCATION_DATA - Success - gpuVA=0x%llX Size=0x%X\n", registrationData.gpuVirtualAddress, registrationData.size);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::handleContextCreateDestroyEvent(DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY_EVENT_PARAMS &contextCreateDestroyParams) {
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_READ_EVENT_CONTEXT_CREATE_DESTROY_EVENT_PARAMS: hContextHandle: 0x%llX IsCreated: %d SIPInstalled: %d\n", contextCreateDestroyParams.hContextHandle, contextCreateDestroyParams.IsCreated, contextCreateDestroyParams.IsSIPInstalled);
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
    PRINT_DEBUGGER_INFO_LOG("DEVICE_CREATE_DESTROY_EVENT: hDeviceContext=0x%llX IsCreated=%d ProcessId=%d\n",
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
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_READ_EVENT_CREATE_DEBUG_DATA_PARAMS: Type: %d BufferPtr: 0x%llX DataSize: 0x%lX\n", createDebugDataParams.DebugDataType, createDebugDataParams.DataBufferPtr, createDebugDataParams.DataSize);
    if (createDebugDataParams.DebugDataType == ELF_BINARY) {
        if (createDebugDataParams.DataBufferPtr && createDebugDataParams.DataSize) {
            std::unique_lock<std::mutex> lock(asyncThreadMutex);
            ElfRange elf = {};
            elf.startVA = createDebugDataParams.DataBufferPtr;
            elf.endVA = elf.startVA + createDebugDataParams.DataSize;
            allElfs.push_back(elf);
        }
    } else if (createDebugDataParams.DebugDataType == static_cast<uint32_t>(NEO::DebugDataType::CMD_QUEUE_CREATED)) {
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_ENTRY;
        pushApiEvent(debugEvent, 0, 0);
    } else if (createDebugDataParams.DebugDataType == static_cast<uint32_t>(NEO::DebugDataType::CMD_QUEUE_DESTROYED)) {
        zet_debug_event_t debugEvent = {};
        debugEvent.type = ZET_DEBUG_EVENT_TYPE_PROCESS_EXIT;
        pushApiEvent(debugEvent, 0, 0);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::acknowledgeEventImp(uint32_t seqNo, uint32_t eventType) {
    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_ACKNOWLEDGE_EVENT: seqNo: %d eventType: %d\n", seqNo, eventType);
    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_ACKNOWLEDGE_EVENT;
    escapeInfo.KmEuDbgL0EscapeInfo.AckEventParams.EventSeqNo = seqNo;
    escapeInfo.KmEuDbgL0EscapeInfo.AckEventParams.ReadEventType = eventType;

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_ACKNOWLEDGE_EVENT: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateNtStatusToZeResult(status);
    }

    if (DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_ACKNOWLEDGE_EVENT: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }

    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_ACKNOWLEDGE_EVENT - Success\n");
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
    case DBGUMD_RETURN_INVALID_EVENT_SEQ_NO:
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

ze_result_t DebugSessionWindows::readElfSpace(const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_READ_UMD_MEMORY;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadUmdMemoryParams.hElfHandle = desc->address;
    escapeInfo.KmEuDbgL0EscapeInfo.ReadUmdMemoryParams.BufferSize = static_cast<uint32_t>(size);
    escapeInfo.KmEuDbgL0EscapeInfo.ReadUmdMemoryParams.BufferPtr = reinterpret_cast<uint64_t>(buffer);

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status || DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_UMD_MEMORY: Failed - ProcessId: %d Status: %d EscapeReturnStatus: %d\n", processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }
    return ZE_RESULT_SUCCESS;
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

    ze_result_t status = validateThreadAndDescForMemoryAccess(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
        status = readDefaultMemory(thread, desc, size, buffer);
    } else {
        auto threadId = convertToThreadId(thread);
        status = slmMemoryAccess<void *, false>(threadId, desc, size, buffer);
    }

    return status;
}

ze_result_t DebugSessionWindows::readDefaultMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, void *buffer) {

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

    ze_result_t status = validateThreadAndDescForMemoryAccess(thread, desc);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (desc->type == ZET_DEBUG_MEMORY_SPACE_TYPE_DEFAULT) {
        status = writeDefaultMemory(thread, desc, size, buffer);
    } else {
        auto threadId = convertToThreadId(thread);
        status = slmMemoryAccess<const void *, true>(threadId, desc, size, buffer);
    }

    return status;
}

ze_result_t DebugSessionWindows::writeDefaultMemory(ze_device_thread_t thread, const zet_debug_memory_space_desc_t *desc, size_t size, const void *buffer) {

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
    std::unique_lock<std::mutex> lock(asyncThreadMutex);

    ze_result_t ret = ZE_RESULT_ERROR_UNKNOWN;
    auto it = std::find_if(eventsToAck.begin(), eventsToAck.end(), [&](auto &e) { return !memcmp(&e, event, sizeof(zet_debug_event_t)); });

    if (it != eventsToAck.end()) {
        ret = acknowledgeEventImp(it->second.first, it->second.second);
        eventsToAck.erase(it);
    }

    return ret;
}

ze_result_t DebugSessionWindows::resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) {
    auto hwInfo = connectedDevice->getHwInfo();
    auto &l0GfxCoreHelper = connectedDevice->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    std::unique_ptr<uint8_t[]> bitmask;
    size_t bitmaskSize = 0;
    l0GfxCoreHelper.getAttentionBitmaskForSingleThreads(threads, hwInfo, bitmask, bitmaskSize);
    printBitmask(bitmask.get(), bitmaskSize);

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_EU_CONTROL_CLR_ATT_BIT;
    escapeInfo.KmEuDbgL0EscapeInfo.EuControlClrAttBitParams.BitmaskArrayPtr = reinterpret_cast<uint64_t>(bitmask.get());
    escapeInfo.KmEuDbgL0EscapeInfo.EuControlClrAttBitParams.BitMaskSizeInBytes = static_cast<uint32_t>(bitmaskSize);

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_EU_CONTROL_CLR_ATT_BIT: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateNtStatusToZeResult(status);
    }

    if (DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_EU_CONTROL_CLR_ATT_BIT: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }

    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_EU_CONTROL_CLR_ATT_BIT - Success\n");
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionWindows::interruptImp(uint32_t deviceIndex) {
    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_EU_CONTROL_INT_ALL;

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_EU_CONTROL_INT_ALL: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    if (DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_EU_CONTROL_INT_ALL: Failed - Status: 0x%llX EscapeReturnStatus: %d\n", status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_EU_CONTROL_INT_ALL - Success\n");
    return ZE_RESULT_SUCCESS;
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
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_GFX_MEMORY: Failed - gpuVA: 0x%llx Size: %zu ProcessId: %d Status: %d EscapeReturnStatus: %d\n", gpuVa, size, processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }
    PRINT_DEBUGGER_MEM_ACCESS_LOG("DBGUMD_ACTION_READ_GFX_MEMORY: Success - gpuVA: 0x%llx Size: %zu ProcessId: %d \n", gpuVa, size, processId);
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
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_WRITE_GFX_MEMORY: Failed - gpuVA: 0x%llx size: %zu ProcessId: %d Status: %d EscapeReturnStatus: %d\n", gpuVa, size, processId, status, escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return DebugSessionWindows::translateEscapeReturnStatusToZeResult(escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
    }
    PRINT_DEBUGGER_MEM_ACCESS_LOG("DBGUMD_ACTION_WRITE_GFX_MEMORY: Success - gpuVA: 0x%llx Size: %zu ProcessId: %d \n", gpuVa, size, processId);
    return ZE_RESULT_SUCCESS;
}

void DebugSessionWindows::enqueueApiEvent(zet_debug_event_t &debugEvent) {
    pushApiEvent(debugEvent, 0, 0);
}

ze_result_t DebugSessionWindows::readSbaBuffer(EuThread::ThreadId threadId, NEO::SbaTrackedAddresses &sbaBuffer) {
    uint64_t gpuVa = 0;
    getSbaBufferGpuVa(gpuVa);
    if (gpuVa == 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    uint64_t memoryHandle = DebugSessionWindows::invalidHandle;
    memoryHandle = allThreads[threadId]->getMemoryHandle();
    if (memoryHandle == EuThread::invalidHandle) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    return readGpuMemory(memoryHandle, reinterpret_cast<char *>(&sbaBuffer), sizeof(sbaBuffer), gpuVa);
}

void DebugSessionWindows::getSbaBufferGpuVa(uint64_t &gpuVa) {
    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.KmEuDbgL0EscapeInfo.EscapeActionType = DBGUMD_ACTION_READ_MMIO;
    escapeInfo.KmEuDbgL0EscapeInfo.MmioReadParams.MmioOffset = CS_GPR_R15;
    escapeInfo.KmEuDbgL0EscapeInfo.MmioReadParams.RegisterOutBufferPtr = reinterpret_cast<uint64_t>(&gpuVa);

    auto status = runEscape(escapeInfo);
    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_MMIO: Escape Failed - Status: %d", status);
        return;
    }
    if (DBGUMD_RETURN_ESCAPE_SUCCESS != escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus) {
        PRINT_DEBUGGER_ERROR_LOG("DBGUMD_ACTION_READ_MMIO: Failed - EscapeReturnStatus: %d\n", escapeInfo.KmEuDbgL0EscapeInfo.EscapeReturnStatus);
        return;
    }

    PRINT_DEBUGGER_INFO_LOG("DBGUMD_ACTION_READ_MMIO: SUCCESS - gpuVa: 0x%llX\n", gpuVa);
    return;
}

bool DebugSessionWindows::readModuleDebugArea() {

    uint64_t memoryHandle = 0;
    uint64_t gpuVa = this->debugAreaVA;
    if (!moduleDebugAreaCaptured || allContexts.empty()) {
        return false;
    }
    memoryHandle = *allContexts.begin();

    memset(this->debugArea.magic, 0, sizeof(this->debugArea.magic));
    auto retVal = readGpuMemory(memoryHandle, reinterpret_cast<char *>(&this->debugArea), sizeof(this->debugArea), gpuVa);

    if (retVal != ZE_RESULT_SUCCESS) {
        PRINT_DEBUGGER_ERROR_LOG("Reading Module Debug Area failed, error = %d\n", retVal);
        return false;
    }

    if (strncmp(this->debugArea.magic, "dbgarea", sizeof(NEO::DebugAreaHeader::magic)) != 0) {
        PRINT_DEBUGGER_ERROR_LOG("Module Debug Area failed to match magic numbers\n");
        return false;
    }

    PRINT_DEBUGGER_INFO_LOG("Reading Module Debug Area Passed\n");
    return true;
}

void DebugSessionWindows::readStateSaveAreaHeader() {

    uint64_t memoryHandle = 0;
    uint64_t gpuVa = 0;
    if (!stateSaveAreaCaptured) {
        return;
    }
    gpuVa = this->stateSaveAreaVA.load();

    {
        std::unique_lock<std::mutex> lock(asyncThreadMutex);
        if (allContexts.empty()) {
            return;
        }
        memoryHandle = *allContexts.begin();
    }

    validateAndSetStateSaveAreaHeader(memoryHandle, gpuVa);
}

} // namespace L0
