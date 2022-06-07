/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include "KmEscape.h"

namespace NEO {

void WddmMemoryManager::registerAllocationInOs(GraphicsAllocation *allocation) {
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

    KM_ESCAPE_INFO escapeInfo;
    memset(&escapeInfo, 0, sizeof(escapeInfo));
    escapeInfo.Header.EscapeCode = GFX_ESCAPE_KMD;
    escapeInfo.Header.Size = sizeof(escapeInfo) - sizeof(escapeInfo.Header);
    escapeInfo.EscapeOperation = KM_ESCAPE_EUDBG_UMD_REGISTER_ALLOCATION_TYPE;
    escapeInfo.KmEuDbgUmdRegisterAllocationData.hAllocation = wddmAllocation->getDefaultHandle();
    escapeInfo.KmEuDbgUmdRegisterAllocationData.hElfAddressPtr = uint64_t(-1);
    escapeInfo.KmEuDbgUmdRegisterAllocationData.NumOfDebugDataInfo = 1;
    escapeInfo.KmEuDbgUmdRegisterAllocationData.DebugDataBufferPtr = reinterpret_cast<uint64_t>(&allocationDebugDataInfo);

    auto &wddm = getWddm(allocation->getRootDeviceIndex());

    D3DKMT_ESCAPE escapeCommand = {0};
    escapeCommand.Flags.HardwareAccess = 0;
    escapeCommand.Flags.Reserved = 0;
    escapeCommand.hAdapter = wddm.getAdapter();
    escapeCommand.hContext = (D3DKMT_HANDLE)0;
    escapeCommand.hDevice = wddm.getDeviceHandle();
    escapeCommand.pPrivateDriverData = &escapeInfo;
    escapeCommand.PrivateDriverDataSize = sizeof(escapeInfo);
    escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

    [[maybe_unused]] auto status = wddm.escape(escapeCommand);
    DEBUG_BREAK_IF(STATUS_SUCCESS != status);
}

} // namespace NEO
