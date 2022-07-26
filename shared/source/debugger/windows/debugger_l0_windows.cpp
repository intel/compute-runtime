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
#include "shared/source/os_interface/windows/wddm_allocation.h"
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

void DebuggerL0::registerAllocationType(GraphicsAllocation *allocation) {
    if (!device->getRootDeviceEnvironment().osInterface) {
        return;
    }

    auto wddm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Wddm>();
    auto dataType = MAX_GFX_ALLOCATION_TYPE;

    auto wddmAllocation = static_cast<WddmAllocation *>(allocation);
    switch (wddmAllocation->getAllocationType()) {
    case AllocationType::DEBUG_CONTEXT_SAVE_AREA:
        dataType = SIP_CONTEXT_SAVE_AREA;
        break;
    case AllocationType::DEBUG_SBA_TRACKING_BUFFER:
        dataType = SBA_BUFFER_AREA;
        break;
    case AllocationType::DEBUG_MODULE_AREA:
        dataType = MODULE_HEAP_DEBUG_AREA;
        break;
    case AllocationType::KERNEL_ISA:
        dataType = ISA;
    default:
        break;
    }

    if (dataType == MAX_GFX_ALLOCATION_TYPE) {
        return;
    }

    WddmAllocation::RegistrationData registrationData = {0};
    registrationData.gpuVirtualAddress = wddmAllocation->getGpuAddress();
    registrationData.size = wddmAllocation->getUnderlyingBufferSize();

    PRINT_DEBUGGER_INFO_LOG("REGISTER_ALLOCATION_TYPE: gpuVA=0x%llX Size=%X DataType=%d DataSize=%d DataPointer=%p\n",
                            registrationData.gpuVirtualAddress, registrationData.size, dataType, sizeof(registrationData), &registrationData);

    GFX_ALLOCATION_DEBUG_DATA_INFO allocationDebugDataInfo;
    allocationDebugDataInfo.DataType = dataType;
    allocationDebugDataInfo.DataSize = sizeof(registrationData);
    allocationDebugDataInfo.DataPointer = reinterpret_cast<uint64_t>(&registrationData);

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.Header.Size = sizeof(escapeInfo) - sizeof(escapeInfo.Header);
    escapeInfo.EscapeOperation = KM_ESCAPE_EUDBG_UMD_REGISTER_ALLOCATION_TYPE;
    escapeInfo.KmEuDbgUmdRegisterAllocationData.hAllocation = wddmAllocation->getDefaultHandle();
    escapeInfo.KmEuDbgUmdRegisterAllocationData.hElfAddressPtr = uint64_t(-1);
    escapeInfo.KmEuDbgUmdRegisterAllocationData.NumOfDebugDataInfo = 1;
    escapeInfo.KmEuDbgUmdRegisterAllocationData.DebugDataBufferPtr = reinterpret_cast<uint64_t>(&allocationDebugDataInfo);

    D3DKMT_ESCAPE escapeCommand = {0};
    escapeCommand.Flags.HardwareAccess = 0;
    escapeCommand.Flags.Reserved = 0;
    escapeCommand.hAdapter = wddm->getAdapter();
    escapeCommand.hContext = (D3DKMT_HANDLE)0;
    escapeCommand.hDevice = wddm->getDeviceHandle();
    escapeCommand.pPrivateDriverData = &escapeInfo;
    escapeCommand.PrivateDriverDataSize = sizeof(escapeInfo);
    escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

    [[maybe_unused]] auto status = wddm->escape(escapeCommand);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);
}

void DebuggerL0::registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *allocation) {
}

bool DebuggerL0::attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &kernelAllocs, uint32_t &moduleHandle) {
    return false;
}

void DebuggerL0::notifyModuleCreate(void *module, uint32_t moduleSize, uint64_t moduleLoadAddress) {
    if (device->getRootDeviceEnvironment().osInterface == nullptr) {
        return;
    }

    // Register ELF
    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.Header.Size = sizeof(escapeInfo) - sizeof(escapeInfo.Header);
    escapeInfo.EscapeOperation = KM_ESCAPE_EUDBG_UMD_CREATE_DEBUG_DATA;
    escapeInfo.KmEuDbgUmdCreateDebugData.DebugDataType = ELF_BINARY;
    escapeInfo.KmEuDbgUmdCreateDebugData.DataSize = moduleSize;
    escapeInfo.KmEuDbgUmdCreateDebugData.hElfAddressPtr = reinterpret_cast<uint64_t>(module);

    auto wddm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Wddm>();

    D3DKMT_ESCAPE escapeCommand = {0};
    escapeCommand.Flags.HardwareAccess = 0;
    escapeCommand.Flags.Reserved = 0;
    escapeCommand.hAdapter = wddm->getAdapter();
    escapeCommand.hContext = (D3DKMT_HANDLE)0;
    escapeCommand.hDevice = wddm->getDeviceHandle();
    escapeCommand.pPrivateDriverData = &escapeInfo;
    escapeCommand.PrivateDriverDataSize = sizeof(escapeInfo);
    escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

    auto status = wddm->escape(escapeCommand);

    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("KM_ESCAPE_EUDBG_UMD_CREATE_DEBUG_DATA: Failed - Status: 0x%llX\n", status);
        return;
    }

    PRINT_DEBUGGER_INFO_LOG("KM_ESCAPE_EUDBG_UMD_CREATE_DEBUG_DATA - Success\n");

    // Fire MODULE_CREATE event
    escapeInfo = {0};
    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.Header.Size = sizeof(escapeInfo) - sizeof(escapeInfo.Header);
    escapeInfo.EscapeOperation = KM_ESCAPE_EUDBG_UMD_MODULE_CREATE_NOTIFY;
    escapeInfo.KmEuDbgUmdCreateModuleNotification.IsCreate = true;
    escapeInfo.KmEuDbgUmdCreateModuleNotification.Modulesize = moduleSize;
    escapeInfo.KmEuDbgUmdCreateModuleNotification.hElfAddressPtr = reinterpret_cast<uint64_t>(module);
    escapeInfo.KmEuDbgUmdCreateModuleNotification.LoadAddress = moduleLoadAddress;

    status = wddm->escape(escapeCommand);

    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("KM_ESCAPE_EUDBG_UMD_MODULE_CREATE_NOTIFY: Failed - Status: 0x%llX\n", status);
        return;
    }

    PRINT_DEBUGGER_INFO_LOG("KM_ESCAPE_EUDBG_UMD_MODULE_CREATE_NOTIFY - Success\n");
}

void DebuggerL0::notifyModuleDestroy(uint64_t moduleLoadAddress) {
    if (device->getRootDeviceEnvironment().osInterface == nullptr) {
        return;
    }

    KM_ESCAPE_INFO escapeInfo = {0};
    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.Header.Size = sizeof(escapeInfo) - sizeof(escapeInfo.Header);
    escapeInfo.EscapeOperation = KM_ESCAPE_EUDBG_UMD_MODULE_CREATE_NOTIFY;
    escapeInfo.KmEuDbgUmdCreateModuleNotification.IsCreate = false;
    escapeInfo.KmEuDbgUmdCreateModuleNotification.Modulesize = 0x1000;
    escapeInfo.KmEuDbgUmdCreateModuleNotification.hElfAddressPtr = uint64_t(-1);
    escapeInfo.KmEuDbgUmdCreateModuleNotification.LoadAddress = moduleLoadAddress;

    auto wddm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Wddm>();

    D3DKMT_ESCAPE escapeCommand = {0};
    escapeCommand.Flags.HardwareAccess = 0;
    escapeCommand.Flags.Reserved = 0;
    escapeCommand.hAdapter = wddm->getAdapter();
    escapeCommand.hContext = (D3DKMT_HANDLE)0;
    escapeCommand.hDevice = wddm->getDeviceHandle();
    escapeCommand.pPrivateDriverData = &escapeInfo;
    escapeCommand.PrivateDriverDataSize = sizeof(escapeInfo);
    escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

    auto status = wddm->escape(escapeCommand);

    if (STATUS_SUCCESS != status) {
        PRINT_DEBUGGER_ERROR_LOG("KM_ESCAPE_EUDBG_UMD_MODULE_DESTROY_NOTIFY: Failed - Status: 0x%llX\n", status);
        return;
    }

    PRINT_DEBUGGER_INFO_LOG("KM_ESCAPE_EUDBG_UMD_MODULE_DESTROY_NOTIFY - Success\n");
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

void DebuggerL0::notifyCommandQueueCreated(NEO::Device *deviceIn) {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);
        if (++commandQueueCount[0] == 1) {
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

void DebuggerL0::notifyCommandQueueDestroyed(NEO::Device *deviceIn) {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);
        if (--commandQueueCount[0] == 0) {
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
