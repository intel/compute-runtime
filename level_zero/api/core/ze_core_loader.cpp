/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/io_functions.h"

#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

#include <stdlib.h>

ze_gpu_driver_dditable_t driverDdiTable;

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDriverProcAddrTable(
    ze_api_version_t version,
    ze_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::globalDriverDispatch.coreDriver.pfnGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetApiVersion, L0::globalDriverDispatch.coreDriver.pfnGetApiVersion, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.coreDriver.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIpcProperties, L0::globalDriverDispatch.coreDriver.pfnGetIpcProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetExtensionProperties, L0::globalDriverDispatch.coreDriver.pfnGetExtensionProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetExtensionFunctionAddress, L0::globalDriverDispatch.coreDriver.pfnGetExtensionFunctionAddress, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnGetLastErrorDescription, L0::globalDriverDispatch.coreDriver.pfnGetLastErrorDescription, version, ZE_API_VERSION_1_6);
    driverDdiTable.coreDdiTable.Driver = *pDdiTable;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetMemProcAddrTable(
    ze_api_version_t version,
    ze_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAllocShared, L0::globalDriverDispatch.coreMem.pfnAllocShared, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAllocDevice, L0::globalDriverDispatch.coreMem.pfnAllocDevice, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAllocHost, L0::globalDriverDispatch.coreMem.pfnAllocHost, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFree, L0::globalDriverDispatch.coreMem.pfnFree, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAllocProperties, L0::globalDriverDispatch.coreMem.pfnGetAllocProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAddressRange, L0::globalDriverDispatch.coreMem.pfnGetAddressRange, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIpcHandle, L0::globalDriverDispatch.coreMem.pfnGetIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOpenIpcHandle, L0::globalDriverDispatch.coreMem.pfnOpenIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCloseIpcHandle, L0::globalDriverDispatch.coreMem.pfnCloseIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFreeExt, L0::globalDriverDispatch.coreMem.pfnFreeExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnPutIpcHandle, L0::globalDriverDispatch.coreMem.pfnPutIpcHandle, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnGetPitchFor2dImage, L0::globalDriverDispatch.coreMem.pfnGetPitchFor2dImage, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.Mem = *pDdiTable;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetContextProcAddrTable(
    ze_api_version_t version,
    ze_context_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreContext.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreContext.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetStatus, L0::globalDriverDispatch.coreContext.pfnGetStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSystemBarrier, L0::globalDriverDispatch.coreContext.pfnSystemBarrier, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnMakeMemoryResident, L0::globalDriverDispatch.coreContext.pfnMakeMemoryResident, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEvictMemory, L0::globalDriverDispatch.coreContext.pfnEvictMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnMakeImageResident, L0::globalDriverDispatch.coreContext.pfnMakeImageResident, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEvictImage, L0::globalDriverDispatch.coreContext.pfnEvictImage, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreateEx, L0::globalDriverDispatch.coreContext.pfnCreateEx, version, ZE_API_VERSION_1_1);

    driverDdiTable.coreDdiTable.Context = *pDdiTable;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetPhysicalMemProcAddrTable(
    ze_api_version_t version,
    ze_physical_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.corePhysicalMem.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.corePhysicalMem.pfnDestroy, version, ZE_API_VERSION_1_0);

    driverDdiTable.coreDdiTable.PhysicalMem = *pDdiTable;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetVirtualMemProcAddrTable(
    ze_api_version_t version,
    ze_virtual_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnReserve, L0::globalDriverDispatch.coreVirtualMem.pfnReserve, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFree, L0::globalDriverDispatch.coreVirtualMem.pfnFree, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryPageSize, L0::globalDriverDispatch.coreVirtualMem.pfnQueryPageSize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnMap, L0::globalDriverDispatch.coreVirtualMem.pfnMap, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnUnmap, L0::globalDriverDispatch.coreVirtualMem.pfnUnmap, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetAccessAttribute, L0::globalDriverDispatch.coreVirtualMem.pfnSetAccessAttribute, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAccessAttribute, L0::globalDriverDispatch.coreVirtualMem.pfnGetAccessAttribute, version, ZE_API_VERSION_1_0);

    driverDdiTable.coreDdiTable.VirtualMem = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetGlobalProcAddrTable(
    ze_api_version_t version,
    ze_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnInit, L0::globalDriverDispatch.coreGlobal.pfnInit, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnInitDrivers, L0::globalDriverDispatch.coreGlobal.pfnInitDrivers, version, ZE_API_VERSION_1_10);
    driverDdiTable.coreDdiTable.Global = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDeviceProcAddrTable(
    ze_api_version_t version,
    ze_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::globalDriverDispatch.coreDevice.pfnGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCommandQueueGroupProperties, L0::globalDriverDispatch.coreDevice.pfnGetCommandQueueGroupProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetSubDevices, L0::globalDriverDispatch.coreDevice.pfnGetSubDevices, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.coreDevice.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetComputeProperties, L0::globalDriverDispatch.coreDevice.pfnGetComputeProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetModuleProperties, L0::globalDriverDispatch.coreDevice.pfnGetModuleProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetMemoryProperties, L0::globalDriverDispatch.coreDevice.pfnGetMemoryProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetMemoryAccessProperties, L0::globalDriverDispatch.coreDevice.pfnGetMemoryAccessProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCacheProperties, L0::globalDriverDispatch.coreDevice.pfnGetCacheProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetImageProperties, L0::globalDriverDispatch.coreDevice.pfnGetImageProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetP2PProperties, L0::globalDriverDispatch.coreDevice.pfnGetP2PProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCanAccessPeer, L0::globalDriverDispatch.coreDevice.pfnCanAccessPeer, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetStatus, L0::globalDriverDispatch.coreDevice.pfnGetStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetExternalMemoryProperties, L0::globalDriverDispatch.coreDevice.pfnGetExternalMemoryProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetGlobalTimestamps, L0::globalDriverDispatch.coreDevice.pfnGetGlobalTimestamps, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnReserveCacheExt, L0::globalDriverDispatch.coreDevice.pfnReserveCacheExt, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnSetCacheAdviceExt, L0::globalDriverDispatch.coreDevice.pfnSetCacheAdviceExt, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnPciGetPropertiesExt, L0::globalDriverDispatch.coreDevice.pfnPciGetPropertiesExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnGetRootDevice, L0::globalDriverDispatch.coreDevice.pfnGetRootDevice, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnImportExternalSemaphoreExt, L0::globalDriverDispatch.coreDevice.pfnImportExternalSemaphoreExt, version, ZE_API_VERSION_1_12);
    fillDdiEntry(pDdiTable->pfnReleaseExternalSemaphoreExt, L0::globalDriverDispatch.coreDevice.pfnReleaseExternalSemaphoreExt, version, ZE_API_VERSION_1_12);
    fillDdiEntry(pDdiTable->pfnGetVectorWidthPropertiesExt, L0::globalDriverDispatch.coreDevice.pfnGetVectorWidthPropertiesExt, version, ZE_API_VERSION_1_13);
    driverDdiTable.coreDdiTable.Device = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDeviceExpProcAddrTable(
    ze_api_version_t version,
    ze_device_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetFabricVertexExp, L0::globalDriverDispatch.coreDeviceExp.pfnGetFabricVertexExp, version, ZE_API_VERSION_1_4);
    driverDdiTable.coreDdiTable.DeviceExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandQueueProcAddrTable(
    ze_api_version_t version,
    ze_command_queue_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreCommandQueue.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreCommandQueue.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnExecuteCommandLists, L0::globalDriverDispatch.coreCommandQueue.pfnExecuteCommandLists, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSynchronize, L0::globalDriverDispatch.coreCommandQueue.pfnSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetOrdinal, L0::globalDriverDispatch.coreCommandQueue.pfnGetOrdinal, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetIndex, L0::globalDriverDispatch.coreCommandQueue.pfnGetIndex, version, ZE_API_VERSION_1_9);

    driverDdiTable.coreDdiTable.CommandQueue = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandListProcAddrTable(
    ze_api_version_t version,
    ze_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAppendBarrier, L0::globalDriverDispatch.coreCommandList.pfnAppendBarrier, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryRangesBarrier, L0::globalDriverDispatch.coreCommandList.pfnAppendMemoryRangesBarrier, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreCommandList.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreateImmediate, L0::globalDriverDispatch.coreCommandList.pfnCreateImmediate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreCommandList.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnClose, L0::globalDriverDispatch.coreCommandList.pfnClose, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::globalDriverDispatch.coreCommandList.pfnReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryCopy, L0::globalDriverDispatch.coreCommandList.pfnAppendMemoryCopy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryCopyRegion, L0::globalDriverDispatch.coreCommandList.pfnAppendMemoryCopyRegion, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryFill, L0::globalDriverDispatch.coreCommandList.pfnAppendMemoryFill, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopy, L0::globalDriverDispatch.coreCommandList.pfnAppendImageCopy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyRegion, L0::globalDriverDispatch.coreCommandList.pfnAppendImageCopyRegion, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyToMemory, L0::globalDriverDispatch.coreCommandList.pfnAppendImageCopyToMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyFromMemory, L0::globalDriverDispatch.coreCommandList.pfnAppendImageCopyFromMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryPrefetch, L0::globalDriverDispatch.coreCommandList.pfnAppendMemoryPrefetch, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemAdvise, L0::globalDriverDispatch.coreCommandList.pfnAppendMemAdvise, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendSignalEvent, L0::globalDriverDispatch.coreCommandList.pfnAppendSignalEvent, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendWaitOnEvents, L0::globalDriverDispatch.coreCommandList.pfnAppendWaitOnEvents, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendEventReset, L0::globalDriverDispatch.coreCommandList.pfnAppendEventReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchKernel, L0::globalDriverDispatch.coreCommandList.pfnAppendLaunchKernel, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchCooperativeKernel, L0::globalDriverDispatch.coreCommandList.pfnAppendLaunchCooperativeKernel, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchKernelIndirect, L0::globalDriverDispatch.coreCommandList.pfnAppendLaunchKernelIndirect, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchMultipleKernelsIndirect, L0::globalDriverDispatch.coreCommandList.pfnAppendLaunchMultipleKernelsIndirect, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendWriteGlobalTimestamp, L0::globalDriverDispatch.coreCommandList.pfnAppendWriteGlobalTimestamp, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryCopyFromContext, L0::globalDriverDispatch.coreCommandList.pfnAppendMemoryCopyFromContext, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendQueryKernelTimestamps, L0::globalDriverDispatch.coreCommandList.pfnAppendQueryKernelTimestamps, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSynchronize, L0::globalDriverDispatch.coreCommandList.pfnHostSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyToMemoryExt, L0::globalDriverDispatch.coreCommandList.pfnAppendImageCopyToMemoryExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyFromMemoryExt, L0::globalDriverDispatch.coreCommandList.pfnAppendImageCopyFromMemoryExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnGetDeviceHandle, L0::globalDriverDispatch.coreCommandList.pfnGetDeviceHandle, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetContextHandle, L0::globalDriverDispatch.coreCommandList.pfnGetContextHandle, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetOrdinal, L0::globalDriverDispatch.coreCommandList.pfnGetOrdinal, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnImmediateGetIndex, L0::globalDriverDispatch.coreCommandList.pfnImmediateGetIndex, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnIsImmediate, L0::globalDriverDispatch.coreCommandList.pfnIsImmediate, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnAppendSignalExternalSemaphoreExt, L0::globalDriverDispatch.coreCommandList.pfnAppendSignalExternalSemaphoreExt, version, ZE_API_VERSION_1_12);
    fillDdiEntry(pDdiTable->pfnAppendWaitExternalSemaphoreExt, L0::globalDriverDispatch.coreCommandList.pfnAppendWaitExternalSemaphoreExt, version, ZE_API_VERSION_1_12);
    driverDdiTable.coreDdiTable.CommandList = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandListExpProcAddrTable(
    ze_api_version_t version,
    ze_command_list_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateCloneExp, L0::globalDriverDispatch.coreCommandListExp.pfnCreateCloneExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnImmediateAppendCommandListsExp, L0::globalDriverDispatch.coreCommandListExp.pfnImmediateAppendCommandListsExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetNextCommandIdExp, L0::globalDriverDispatch.coreCommandListExp.pfnGetNextCommandIdExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandsExp, L0::globalDriverDispatch.coreCommandListExp.pfnUpdateMutableCommandsExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandSignalEventExp, L0::globalDriverDispatch.coreCommandListExp.pfnUpdateMutableCommandSignalEventExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandWaitEventsExp, L0::globalDriverDispatch.coreCommandListExp.pfnUpdateMutableCommandWaitEventsExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetNextCommandIdWithKernelsExp, L0::globalDriverDispatch.coreCommandListExp.pfnGetNextCommandIdWithKernelsExp, version, ZE_API_VERSION_1_10);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandKernelsExp, L0::globalDriverDispatch.coreCommandListExp.pfnUpdateMutableCommandKernelsExp, version, ZE_API_VERSION_1_10);
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFenceProcAddrTable(
    ze_api_version_t version,
    ze_fence_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreFence.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreFence.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSynchronize, L0::globalDriverDispatch.coreFence.pfnHostSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryStatus, L0::globalDriverDispatch.coreFence.pfnQueryStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::globalDriverDispatch.coreFence.pfnReset, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Fence = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventPoolProcAddrTable(
    ze_api_version_t version,
    ze_event_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreEventPool.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreEventPool.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIpcHandle, L0::globalDriverDispatch.coreEventPool.pfnGetIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOpenIpcHandle, L0::globalDriverDispatch.coreEventPool.pfnOpenIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCloseIpcHandle, L0::globalDriverDispatch.coreEventPool.pfnCloseIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnPutIpcHandle, L0::globalDriverDispatch.coreEventPool.pfnPutIpcHandle, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnGetContextHandle, L0::globalDriverDispatch.coreEventPool.pfnGetContextHandle, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetFlags, L0::globalDriverDispatch.coreEventPool.pfnGetFlags, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.EventPool = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventProcAddrTable(
    ze_api_version_t version,
    ze_event_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreEvent.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreEvent.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSignal, L0::globalDriverDispatch.coreEvent.pfnHostSignal, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSynchronize, L0::globalDriverDispatch.coreEvent.pfnHostSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryStatus, L0::globalDriverDispatch.coreEvent.pfnQueryStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostReset, L0::globalDriverDispatch.coreEvent.pfnHostReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryKernelTimestamp, L0::globalDriverDispatch.coreEvent.pfnQueryKernelTimestamp, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryKernelTimestampsExt, L0::globalDriverDispatch.coreEvent.pfnQueryKernelTimestampsExt, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnGetEventPool, L0::globalDriverDispatch.coreEvent.pfnGetEventPool, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetSignalScope, L0::globalDriverDispatch.coreEvent.pfnGetSignalScope, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetWaitScope, L0::globalDriverDispatch.coreEvent.pfnGetWaitScope, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.Event = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventExpProcAddrTable(
    ze_api_version_t version,
    ze_event_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnQueryTimestampsExp, L0::globalDriverDispatch.coreEventExp.pfnQueryTimestampsExp, version, ZE_API_VERSION_1_2);

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageProcAddrTable(
    ze_api_version_t version,
    ze_image_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.coreImage.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreImage.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreImage.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAllocPropertiesExt, L0::globalDriverDispatch.coreImage.pfnGetAllocPropertiesExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnViewCreateExt, L0::globalDriverDispatch.coreImage.pfnViewCreateExt, version, ZE_API_VERSION_1_5);
    driverDdiTable.coreDdiTable.Image = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleProcAddrTable(
    ze_api_version_t version,
    ze_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreModule.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreModule.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDynamicLink, L0::globalDriverDispatch.coreModule.pfnDynamicLink, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetNativeBinary, L0::globalDriverDispatch.coreModule.pfnGetNativeBinary, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetGlobalPointer, L0::globalDriverDispatch.coreModule.pfnGetGlobalPointer, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetKernelNames, L0::globalDriverDispatch.coreModule.pfnGetKernelNames, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetFunctionPointer, L0::globalDriverDispatch.coreModule.pfnGetFunctionPointer, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.coreModule.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnInspectLinkageExt, L0::globalDriverDispatch.coreModule.pfnInspectLinkageExt, version, ZE_API_VERSION_1_3);
    driverDdiTable.coreDdiTable.Module = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleBuildLogProcAddrTable(
    ze_api_version_t version,
    ze_module_build_log_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreModuleBuildLog.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetString, L0::globalDriverDispatch.coreModuleBuildLog.pfnGetString, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.ModuleBuildLog = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetKernelProcAddrTable(
    ze_api_version_t version,
    ze_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreKernel.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreKernel.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetGroupSize, L0::globalDriverDispatch.coreKernel.pfnSetGroupSize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSuggestGroupSize, L0::globalDriverDispatch.coreKernel.pfnSuggestGroupSize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSuggestMaxCooperativeGroupCount, L0::globalDriverDispatch.coreKernel.pfnSuggestMaxCooperativeGroupCount, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetArgumentValue, L0::globalDriverDispatch.coreKernel.pfnSetArgumentValue, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetIndirectAccess, L0::globalDriverDispatch.coreKernel.pfnSetIndirectAccess, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIndirectAccess, L0::globalDriverDispatch.coreKernel.pfnGetIndirectAccess, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetSourceAttributes, L0::globalDriverDispatch.coreKernel.pfnGetSourceAttributes, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.coreKernel.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetCacheConfig, L0::globalDriverDispatch.coreKernel.pfnSetCacheConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetName, L0::globalDriverDispatch.coreKernel.pfnGetName, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Kernel = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetSamplerProcAddrTable(
    ze_api_version_t version,
    ze_sampler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.coreSampler.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.coreSampler.pfnDestroy, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Sampler = *pDdiTable;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetKernelExpProcAddrTable(
    ze_api_version_t version,
    ze_kernel_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnSetGlobalOffsetExp, L0::globalDriverDispatch.coreKernelExp.pfnSetGlobalOffsetExp, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnSchedulingHintExp, L0::globalDriverDispatch.coreKernelExp.pfnSchedulingHintExp, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnGetBinaryExp, L0::globalDriverDispatch.coreKernelExp.pfnGetBinaryExp, version, ZE_API_VERSION_1_11);
    driverDdiTable.coreDdiTable.KernelExp = *pDdiTable;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetMemExpProcAddrTable(
    ze_api_version_t version,
    ze_mem_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetIpcHandleFromFileDescriptorExp, L0::globalDriverDispatch.coreMemExp.pfnGetIpcHandleFromFileDescriptorExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnGetFileDescriptorFromIpcHandleExp, L0::globalDriverDispatch.coreMemExp.pfnGetFileDescriptorFromIpcHandleExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnSetAtomicAccessAttributeExp, L0::globalDriverDispatch.coreMemExp.pfnSetAtomicAccessAttributeExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetAtomicAccessAttributeExp, L0::globalDriverDispatch.coreMemExp.pfnGetAtomicAccessAttributeExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.MemExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageExpProcAddrTable(
    ze_api_version_t version,
    ze_image_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetMemoryPropertiesExp, L0::globalDriverDispatch.coreImageExp.pfnGetMemoryPropertiesExp, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnViewCreateExp, L0::globalDriverDispatch.coreImageExp.pfnViewCreateExp, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnGetDeviceOffsetExp, L0::globalDriverDispatch.coreImageExp.pfnGetDeviceOffsetExp, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.ImageExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFabricVertexExpProcAddrTable(
    ze_api_version_t version,
    ze_fabric_vertex_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetExp, L0::globalDriverDispatch.coreFabricVertexExp.pfnGetExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetSubVerticesExp, L0::globalDriverDispatch.coreFabricVertexExp.pfnGetSubVerticesExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::globalDriverDispatch.coreFabricVertexExp.pfnGetPropertiesExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetDeviceExp, L0::globalDriverDispatch.coreFabricVertexExp.pfnGetDeviceExp, version, ZE_API_VERSION_1_4);
    driverDdiTable.coreDdiTable.FabricVertexExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFabricEdgeExpProcAddrTable(
    ze_api_version_t version,
    ze_fabric_edge_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetExp, L0::globalDriverDispatch.coreFabricEdgeExp.pfnGetExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetVerticesExp, L0::globalDriverDispatch.coreFabricEdgeExp.pfnGetVerticesExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::globalDriverDispatch.coreFabricEdgeExp.pfnGetPropertiesExp, version, ZE_API_VERSION_1_4);
    driverDdiTable.coreDdiTable.FabricEdgeExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDriverExpProcAddrTable(
    ze_api_version_t version,
    ze_driver_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnRTASFormatCompatibilityCheckExp, L0::globalDriverDispatch.coreDriverExp.pfnRTASFormatCompatibilityCheckExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.DriverExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASParallelOperationExpProcAddrTable(
    ze_api_version_t version,
    ze_rtas_parallel_operation_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateExp, L0::globalDriverDispatch.coreRTASParallelOperationExp.pfnCreateExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::globalDriverDispatch.coreRTASParallelOperationExp.pfnGetPropertiesExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnJoinExp, L0::globalDriverDispatch.coreRTASParallelOperationExp.pfnJoinExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::globalDriverDispatch.coreRTASParallelOperationExp.pfnDestroyExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.RTASParallelOperationExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASBuilderExpProcAddrTable(
    ze_api_version_t version,
    ze_rtas_builder_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateExp, L0::globalDriverDispatch.coreRTASBuilderExp.pfnCreateExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetBuildPropertiesExp, L0::globalDriverDispatch.coreRTASBuilderExp.pfnGetBuildPropertiesExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnBuildExp, L0::globalDriverDispatch.coreRTASBuilderExp.pfnBuildExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::globalDriverDispatch.coreRTASBuilderExp.pfnDestroyExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.RTASBuilderExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASBuilderProcAddrTable(
    ze_api_version_t version,
    ze_rtas_builder_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateExt, L0::globalDriverDispatch.coreRTASBuilder.pfnCreateExt, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnGetBuildPropertiesExt, L0::globalDriverDispatch.coreRTASBuilder.pfnGetBuildPropertiesExt, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnBuildExt, L0::globalDriverDispatch.coreRTASBuilder.pfnBuildExt, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnCommandListAppendCopyExt, L0::globalDriverDispatch.coreRTASBuilder.pfnCommandListAppendCopyExt, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnDestroyExt, L0::globalDriverDispatch.coreRTASBuilder.pfnDestroyExt, version, ZE_API_VERSION_1_13);
    driverDdiTable.coreDdiTable.RTASBuilder = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASParallelOperationProcAddrTable(
    ze_api_version_t version,
    ze_rtas_parallel_operation_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.core.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateExt, L0::globalDriverDispatch.coreRTASParallelOperation.pfnCreateExt, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExt, L0::globalDriverDispatch.coreRTASParallelOperation.pfnGetPropertiesExt, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnJoinExt, L0::globalDriverDispatch.coreRTASParallelOperation.pfnJoinExt, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnDestroyExt, L0::globalDriverDispatch.coreRTASParallelOperation.pfnDestroyExt, version, ZE_API_VERSION_1_13);
    driverDdiTable.coreDdiTable.RTASParallelOperation = *pDdiTable;
    return result;
}
