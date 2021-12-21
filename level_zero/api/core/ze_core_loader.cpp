/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"
#include "level_zero/source/inc/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"

ze_gpu_driver_dditable_t driver_ddiTable;

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDriverProcAddrTable(
    ze_api_version_t version,
    ze_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = zeDriverGet;
    pDdiTable->pfnGetApiVersion = zeDriverGetApiVersion;
    pDdiTable->pfnGetProperties = zeDriverGetProperties;
    pDdiTable->pfnGetIpcProperties = zeDriverGetIpcProperties;
    pDdiTable->pfnGetExtensionProperties = zeDriverGetExtensionProperties;
    pDdiTable->pfnGetExtensionFunctionAddress = zeDriverGetExtensionFunctionAddress;
    driver_ddiTable.core_ddiTable.Driver = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGet = zeDriverGet_Tracing;
        pDdiTable->pfnGetApiVersion = zeDriverGetApiVersion_Tracing;
        pDdiTable->pfnGetProperties = zeDriverGetProperties_Tracing;
        pDdiTable->pfnGetIpcProperties = zeDriverGetIpcProperties_Tracing;
        pDdiTable->pfnGetExtensionProperties = zeDriverGetExtensionProperties_Tracing;
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetMemProcAddrTable(
    ze_api_version_t version,
    ze_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAllocShared = zeMemAllocShared;
    pDdiTable->pfnAllocDevice = zeMemAllocDevice;
    pDdiTable->pfnAllocHost = zeMemAllocHost;
    pDdiTable->pfnFree = zeMemFree;
    pDdiTable->pfnFreeExt = zeMemFreeExt;
    pDdiTable->pfnGetAllocProperties = zeMemGetAllocProperties;
    pDdiTable->pfnGetAddressRange = zeMemGetAddressRange;
    pDdiTable->pfnGetIpcHandle = zeMemGetIpcHandle;
    pDdiTable->pfnOpenIpcHandle = zeMemOpenIpcHandle;
    pDdiTable->pfnCloseIpcHandle = zeMemCloseIpcHandle;
    driver_ddiTable.core_ddiTable.Mem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnAllocShared = zeMemAllocShared_Tracing;
        pDdiTable->pfnAllocDevice = zeMemAllocDevice_Tracing;
        pDdiTable->pfnAllocHost = zeMemAllocHost_Tracing;
        pDdiTable->pfnFree = zeMemFree_Tracing;
        pDdiTable->pfnGetAllocProperties = zeMemGetAllocProperties_Tracing;
        pDdiTable->pfnGetAddressRange = zeMemGetAddressRange_Tracing;
        pDdiTable->pfnGetIpcHandle = zeMemGetIpcHandle_Tracing;
        pDdiTable->pfnOpenIpcHandle = zeMemOpenIpcHandle_Tracing;
        pDdiTable->pfnCloseIpcHandle = zeMemCloseIpcHandle_Tracing;
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetContextProcAddrTable(
    ze_api_version_t version,
    ze_context_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeContextCreate;
    pDdiTable->pfnCreateEx = zeContextCreateEx;
    pDdiTable->pfnDestroy = zeContextDestroy;
    pDdiTable->pfnGetStatus = zeContextGetStatus;
    pDdiTable->pfnSystemBarrier = zeContextSystemBarrier;
    pDdiTable->pfnMakeMemoryResident = zeContextMakeMemoryResident;
    pDdiTable->pfnEvictMemory = zeContextEvictMemory;
    pDdiTable->pfnMakeImageResident = zeContextMakeImageResident;
    pDdiTable->pfnEvictImage = zeContextEvictImage;

    driver_ddiTable.core_ddiTable.Context = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeContextCreate_Tracing;
        pDdiTable->pfnDestroy = zeContextDestroy_Tracing;
        pDdiTable->pfnGetStatus = zeContextGetStatus_Tracing;
        pDdiTable->pfnSystemBarrier = zeContextSystemBarrier_Tracing;
        pDdiTable->pfnMakeMemoryResident = zeContextMakeMemoryResident_Tracing;
        pDdiTable->pfnEvictMemory = zeContextEvictMemory_Tracing;
        pDdiTable->pfnMakeImageResident = zeContextMakeImageResident_Tracing;
        pDdiTable->pfnEvictImage = zeContextEvictImage_Tracing;
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetPhysicalMemProcAddrTable(
    ze_api_version_t version,
    ze_physical_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zePhysicalMemCreate;
    pDdiTable->pfnDestroy = zePhysicalMemDestroy;

    driver_ddiTable.core_ddiTable.PhysicalMem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zePhysicalMemCreate_Tracing;
        pDdiTable->pfnDestroy = zePhysicalMemDestroy_Tracing;
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetVirtualMemProcAddrTable(
    ze_api_version_t version,
    ze_virtual_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnReserve = zeVirtualMemReserve;
    pDdiTable->pfnFree = zeVirtualMemFree;
    pDdiTable->pfnQueryPageSize = zeVirtualMemQueryPageSize;
    pDdiTable->pfnMap = zeVirtualMemMap;
    pDdiTable->pfnUnmap = zeVirtualMemUnmap;
    pDdiTable->pfnSetAccessAttribute = zeVirtualMemSetAccessAttribute;
    pDdiTable->pfnGetAccessAttribute = zeVirtualMemGetAccessAttribute;

    driver_ddiTable.core_ddiTable.VirtualMem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnReserve = zeVirtualMemReserve_Tracing;
        pDdiTable->pfnFree = zeVirtualMemFree_Tracing;
        pDdiTable->pfnQueryPageSize = zeVirtualMemQueryPageSize_Tracing;
        pDdiTable->pfnMap = zeVirtualMemMap_Tracing;
        pDdiTable->pfnUnmap = zeVirtualMemUnmap_Tracing;
        pDdiTable->pfnSetAccessAttribute = zeVirtualMemSetAccessAttribute_Tracing;
        pDdiTable->pfnGetAccessAttribute = zeVirtualMemGetAccessAttribute_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetGlobalProcAddrTable(
    ze_api_version_t version,
    ze_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnInit = zeInit;
    driver_ddiTable.core_ddiTable.Global = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnInit = zeInit_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDeviceProcAddrTable(
    ze_api_version_t version,
    ze_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = zeDeviceGet;
    pDdiTable->pfnGetCommandQueueGroupProperties = zeDeviceGetCommandQueueGroupProperties;
    pDdiTable->pfnGetSubDevices = zeDeviceGetSubDevices;
    pDdiTable->pfnGetProperties = zeDeviceGetProperties;
    pDdiTable->pfnGetComputeProperties = zeDeviceGetComputeProperties;
    pDdiTable->pfnGetModuleProperties = zeDeviceGetModuleProperties;
    pDdiTable->pfnGetMemoryProperties = zeDeviceGetMemoryProperties;
    pDdiTable->pfnGetMemoryAccessProperties = zeDeviceGetMemoryAccessProperties;
    pDdiTable->pfnGetCacheProperties = zeDeviceGetCacheProperties;
    pDdiTable->pfnGetImageProperties = zeDeviceGetImageProperties;
    pDdiTable->pfnGetP2PProperties = zeDeviceGetP2PProperties;
    pDdiTable->pfnCanAccessPeer = zeDeviceCanAccessPeer;
    pDdiTable->pfnGetStatus = zeDeviceGetStatus;
    pDdiTable->pfnGetExternalMemoryProperties = zeDeviceGetExternalMemoryProperties;
    pDdiTable->pfnGetGlobalTimestamps = zeDeviceGetGlobalTimestamps;
    pDdiTable->pfnReserveCacheExt = zeDeviceReserveCacheExt;
    pDdiTable->pfnSetCacheAdviceExt = zeDeviceSetCacheAdviceExt;
    pDdiTable->pfnPciGetPropertiesExt = zeDevicePciGetPropertiesExt;
    driver_ddiTable.core_ddiTable.Device = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGet = zeDeviceGet_Tracing;
        pDdiTable->pfnGetCommandQueueGroupProperties = zeDeviceGetCommandQueueGroupProperties_Tracing;
        pDdiTable->pfnGetSubDevices = zeDeviceGetSubDevices_Tracing;
        pDdiTable->pfnGetProperties = zeDeviceGetProperties_Tracing;
        pDdiTable->pfnGetComputeProperties = zeDeviceGetComputeProperties_Tracing;
        pDdiTable->pfnGetModuleProperties = zeDeviceGetModuleProperties_Tracing;
        pDdiTable->pfnGetMemoryProperties = zeDeviceGetMemoryProperties_Tracing;
        pDdiTable->pfnGetMemoryAccessProperties = zeDeviceGetMemoryAccessProperties_Tracing;
        pDdiTable->pfnGetCacheProperties = zeDeviceGetCacheProperties_Tracing;
        pDdiTable->pfnGetImageProperties = zeDeviceGetImageProperties_Tracing;
        pDdiTable->pfnGetP2PProperties = zeDeviceGetP2PProperties_Tracing;
        pDdiTable->pfnCanAccessPeer = zeDeviceCanAccessPeer_Tracing;
        pDdiTable->pfnGetStatus = zeDeviceGetStatus_Tracing;
        pDdiTable->pfnGetExternalMemoryProperties = zeDeviceGetExternalMemoryProperties_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandQueueProcAddrTable(
    ze_api_version_t version,
    ze_command_queue_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeCommandQueueCreate;
    pDdiTable->pfnDestroy = zeCommandQueueDestroy;
    pDdiTable->pfnExecuteCommandLists = zeCommandQueueExecuteCommandLists;
    pDdiTable->pfnSynchronize = zeCommandQueueSynchronize;
    driver_ddiTable.core_ddiTable.CommandQueue = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeCommandQueueCreate_Tracing;
        pDdiTable->pfnDestroy = zeCommandQueueDestroy_Tracing;
        pDdiTable->pfnExecuteCommandLists = zeCommandQueueExecuteCommandLists_Tracing;
        pDdiTable->pfnSynchronize = zeCommandQueueSynchronize_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandListProcAddrTable(
    ze_api_version_t version,
    ze_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAppendBarrier = zeCommandListAppendBarrier;
    pDdiTable->pfnAppendMemoryRangesBarrier = zeCommandListAppendMemoryRangesBarrier;
    pDdiTable->pfnCreate = zeCommandListCreate;
    pDdiTable->pfnCreateImmediate = zeCommandListCreateImmediate;
    pDdiTable->pfnDestroy = zeCommandListDestroy;
    pDdiTable->pfnClose = zeCommandListClose;
    pDdiTable->pfnReset = zeCommandListReset;
    pDdiTable->pfnAppendMemoryCopy = zeCommandListAppendMemoryCopy;
    pDdiTable->pfnAppendMemoryCopyRegion = zeCommandListAppendMemoryCopyRegion;
    pDdiTable->pfnAppendMemoryFill = zeCommandListAppendMemoryFill;
    pDdiTable->pfnAppendImageCopy = zeCommandListAppendImageCopy;
    pDdiTable->pfnAppendImageCopyRegion = zeCommandListAppendImageCopyRegion;
    pDdiTable->pfnAppendImageCopyToMemory = zeCommandListAppendImageCopyToMemory;
    pDdiTable->pfnAppendImageCopyFromMemory = zeCommandListAppendImageCopyFromMemory;
    pDdiTable->pfnAppendMemoryPrefetch = zeCommandListAppendMemoryPrefetch;
    pDdiTable->pfnAppendMemAdvise = zeCommandListAppendMemAdvise;
    pDdiTable->pfnAppendSignalEvent = zeCommandListAppendSignalEvent;
    pDdiTable->pfnAppendWaitOnEvents = zeCommandListAppendWaitOnEvents;
    pDdiTable->pfnAppendEventReset = zeCommandListAppendEventReset;
    pDdiTable->pfnAppendLaunchKernel = zeCommandListAppendLaunchKernel;
    pDdiTable->pfnAppendLaunchCooperativeKernel = zeCommandListAppendLaunchCooperativeKernel;
    pDdiTable->pfnAppendLaunchKernelIndirect = zeCommandListAppendLaunchKernelIndirect;
    pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = zeCommandListAppendLaunchMultipleKernelsIndirect;
    pDdiTable->pfnAppendWriteGlobalTimestamp = zeCommandListAppendWriteGlobalTimestamp;
    pDdiTable->pfnAppendMemoryCopyFromContext = zeCommandListAppendMemoryCopyFromContext;
    pDdiTable->pfnAppendQueryKernelTimestamps = zeCommandListAppendQueryKernelTimestamps;
    driver_ddiTable.core_ddiTable.CommandList = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnAppendBarrier = zeCommandListAppendBarrier_Tracing;
        pDdiTable->pfnAppendMemoryRangesBarrier = zeCommandListAppendMemoryRangesBarrier_Tracing;
        pDdiTable->pfnCreate = zeCommandListCreate_Tracing;
        pDdiTable->pfnCreateImmediate = zeCommandListCreateImmediate_Tracing;
        pDdiTable->pfnDestroy = zeCommandListDestroy_Tracing;
        pDdiTable->pfnClose = zeCommandListClose_Tracing;
        pDdiTable->pfnReset = zeCommandListReset_Tracing;
        pDdiTable->pfnAppendMemoryCopy = zeCommandListAppendMemoryCopy_Tracing;
        pDdiTable->pfnAppendMemoryCopyRegion = zeCommandListAppendMemoryCopyRegion_Tracing;
        pDdiTable->pfnAppendMemoryFill = zeCommandListAppendMemoryFill_Tracing;
        pDdiTable->pfnAppendImageCopy = zeCommandListAppendImageCopy_Tracing;
        pDdiTable->pfnAppendImageCopyRegion = zeCommandListAppendImageCopyRegion_Tracing;
        pDdiTable->pfnAppendImageCopyToMemory = zeCommandListAppendImageCopyToMemory_Tracing;
        pDdiTable->pfnAppendImageCopyFromMemory = zeCommandListAppendImageCopyFromMemory_Tracing;
        pDdiTable->pfnAppendMemoryPrefetch = zeCommandListAppendMemoryPrefetch_Tracing;
        pDdiTable->pfnAppendMemAdvise = zeCommandListAppendMemAdvise_Tracing;
        pDdiTable->pfnAppendSignalEvent = zeCommandListAppendSignalEvent_Tracing;
        pDdiTable->pfnAppendWaitOnEvents = zeCommandListAppendWaitOnEvents_Tracing;
        pDdiTable->pfnAppendEventReset = zeCommandListAppendEventReset_Tracing;
        pDdiTable->pfnAppendLaunchKernel = zeCommandListAppendLaunchKernel_Tracing;
        pDdiTable->pfnAppendLaunchCooperativeKernel = zeCommandListAppendLaunchCooperativeKernel_Tracing;
        pDdiTable->pfnAppendLaunchKernelIndirect = zeCommandListAppendLaunchKernelIndirect_Tracing;
        pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = zeCommandListAppendLaunchMultipleKernelsIndirect_Tracing;
        pDdiTable->pfnAppendWriteGlobalTimestamp = zeCommandListAppendWriteGlobalTimestamp_Tracing;
        pDdiTable->pfnAppendMemoryCopyFromContext = zeCommandListAppendMemoryCopyFromContext_Tracing;
        pDdiTable->pfnAppendQueryKernelTimestamps = zeCommandListAppendQueryKernelTimestamps_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFenceProcAddrTable(
    ze_api_version_t version,
    ze_fence_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeFenceCreate;
    pDdiTable->pfnDestroy = zeFenceDestroy;
    pDdiTable->pfnHostSynchronize = zeFenceHostSynchronize;
    pDdiTable->pfnQueryStatus = zeFenceQueryStatus;
    pDdiTable->pfnReset = zeFenceReset;
    driver_ddiTable.core_ddiTable.Fence = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeFenceCreate_Tracing;
        pDdiTable->pfnDestroy = zeFenceDestroy_Tracing;
        pDdiTable->pfnHostSynchronize = zeFenceHostSynchronize_Tracing;
        pDdiTable->pfnQueryStatus = zeFenceQueryStatus_Tracing;
        pDdiTable->pfnReset = zeFenceReset_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventPoolProcAddrTable(
    ze_api_version_t version,
    ze_event_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeEventPoolCreate;
    pDdiTable->pfnDestroy = zeEventPoolDestroy;
    pDdiTable->pfnGetIpcHandle = zeEventPoolGetIpcHandle;
    pDdiTable->pfnOpenIpcHandle = zeEventPoolOpenIpcHandle;
    pDdiTable->pfnCloseIpcHandle = zeEventPoolCloseIpcHandle;
    driver_ddiTable.core_ddiTable.EventPool = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeEventPoolCreate_Tracing;
        pDdiTable->pfnDestroy = zeEventPoolDestroy_Tracing;
        pDdiTable->pfnGetIpcHandle = zeEventPoolGetIpcHandle_Tracing;
        pDdiTable->pfnOpenIpcHandle = zeEventPoolOpenIpcHandle_Tracing;
        pDdiTable->pfnCloseIpcHandle = zeEventPoolCloseIpcHandle_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventProcAddrTable(
    ze_api_version_t version,
    ze_event_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeEventCreate;
    pDdiTable->pfnDestroy = zeEventDestroy;
    pDdiTable->pfnHostSignal = zeEventHostSignal;
    pDdiTable->pfnHostSynchronize = zeEventHostSynchronize;
    pDdiTable->pfnQueryStatus = zeEventQueryStatus;
    pDdiTable->pfnHostReset = zeEventHostReset;
    pDdiTable->pfnQueryKernelTimestamp = zeEventQueryKernelTimestamp;
    driver_ddiTable.core_ddiTable.Event = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeEventCreate_Tracing;
        pDdiTable->pfnDestroy = zeEventDestroy_Tracing;
        pDdiTable->pfnHostSignal = zeEventHostSignal_Tracing;
        pDdiTable->pfnHostSynchronize = zeEventHostSynchronize_Tracing;
        pDdiTable->pfnQueryStatus = zeEventQueryStatus_Tracing;
        pDdiTable->pfnHostReset = zeEventHostReset_Tracing;
        pDdiTable->pfnQueryKernelTimestamp = zeEventQueryKernelTimestamp_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventExpProcAddrTable(
    ze_api_version_t version,
    ze_event_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnQueryTimestampsExp = zeEventQueryTimestampsExp;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageProcAddrTable(
    ze_api_version_t version,
    ze_image_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = zeImageGetProperties;
    pDdiTable->pfnCreate = zeImageCreate;
    pDdiTable->pfnDestroy = zeImageDestroy;
    driver_ddiTable.core_ddiTable.Image = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGetProperties = zeImageGetProperties_Tracing;
        pDdiTable->pfnCreate = zeImageCreate_Tracing;
        pDdiTable->pfnDestroy = zeImageDestroy_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleProcAddrTable(
    ze_api_version_t version,
    ze_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeModuleCreate;
    pDdiTable->pfnDestroy = zeModuleDestroy;
    pDdiTable->pfnDynamicLink = zeModuleDynamicLink;
    pDdiTable->pfnGetNativeBinary = zeModuleGetNativeBinary;
    pDdiTable->pfnGetGlobalPointer = zeModuleGetGlobalPointer;
    pDdiTable->pfnGetKernelNames = zeModuleGetKernelNames;
    pDdiTable->pfnGetFunctionPointer = zeModuleGetFunctionPointer;
    pDdiTable->pfnGetProperties = zeModuleGetProperties;
    driver_ddiTable.core_ddiTable.Module = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeModuleCreate_Tracing;
        pDdiTable->pfnDestroy = zeModuleDestroy_Tracing;
        pDdiTable->pfnGetNativeBinary = zeModuleGetNativeBinary_Tracing;
        pDdiTable->pfnDynamicLink = zeModuleDynamicLink_Tracing;
        pDdiTable->pfnGetGlobalPointer = zeModuleGetGlobalPointer_Tracing;
        pDdiTable->pfnGetFunctionPointer = zeModuleGetFunctionPointer_Tracing;
        pDdiTable->pfnGetKernelNames = zeModuleGetKernelNames_Tracing;
        pDdiTable->pfnGetProperties = zeModuleGetProperties_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleBuildLogProcAddrTable(
    ze_api_version_t version,
    ze_module_build_log_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnDestroy = zeModuleBuildLogDestroy;
    pDdiTable->pfnGetString = zeModuleBuildLogGetString;
    driver_ddiTable.core_ddiTable.ModuleBuildLog = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnDestroy = zeModuleBuildLogDestroy_Tracing;
        pDdiTable->pfnGetString = zeModuleBuildLogGetString_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetKernelProcAddrTable(
    ze_api_version_t version,
    ze_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeKernelCreate;
    pDdiTable->pfnDestroy = zeKernelDestroy;
    pDdiTable->pfnSetGroupSize = zeKernelSetGroupSize;
    pDdiTable->pfnSuggestGroupSize = zeKernelSuggestGroupSize;
    pDdiTable->pfnSuggestMaxCooperativeGroupCount = zeKernelSuggestMaxCooperativeGroupCount;
    pDdiTable->pfnSetArgumentValue = zeKernelSetArgumentValue;
    pDdiTable->pfnSetIndirectAccess = zeKernelSetIndirectAccess;
    pDdiTable->pfnGetIndirectAccess = zeKernelGetIndirectAccess;
    pDdiTable->pfnGetSourceAttributes = zeKernelGetSourceAttributes;
    pDdiTable->pfnGetProperties = zeKernelGetProperties;
    pDdiTable->pfnSetCacheConfig = zeKernelSetCacheConfig;
    pDdiTable->pfnGetName = zeKernelGetName;
    driver_ddiTable.core_ddiTable.Kernel = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeKernelCreate_Tracing;
        pDdiTable->pfnDestroy = zeKernelDestroy_Tracing;
        pDdiTable->pfnSetGroupSize = zeKernelSetGroupSize_Tracing;
        pDdiTable->pfnSuggestGroupSize = zeKernelSuggestGroupSize_Tracing;
        pDdiTable->pfnSuggestMaxCooperativeGroupCount = zeKernelSuggestMaxCooperativeGroupCount_Tracing;
        pDdiTable->pfnSetArgumentValue = zeKernelSetArgumentValue_Tracing;
        pDdiTable->pfnSetIndirectAccess = zeKernelSetIndirectAccess_Tracing;
        pDdiTable->pfnGetIndirectAccess = zeKernelGetIndirectAccess_Tracing;
        pDdiTable->pfnGetSourceAttributes = zeKernelGetSourceAttributes_Tracing;
        pDdiTable->pfnGetProperties = zeKernelGetProperties_Tracing;
        pDdiTable->pfnSetCacheConfig = zeKernelSetCacheConfig_Tracing;
        pDdiTable->pfnGetName = zeKernelGetName_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetSamplerProcAddrTable(
    ze_api_version_t version,
    ze_sampler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeSamplerCreate;
    pDdiTable->pfnDestroy = zeSamplerDestroy;
    driver_ddiTable.core_ddiTable.Sampler = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeSamplerCreate_Tracing;
        pDdiTable->pfnDestroy = zeSamplerDestroy_Tracing;
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetKernelExpProcAddrTable(
    ze_api_version_t version,
    ze_kernel_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnSetGlobalOffsetExp = zeKernelSetGlobalOffsetExp;
    pDdiTable->pfnSchedulingHintExp = zeKernelSchedulingHintExp;
    driver_ddiTable.core_ddiTable.KernelExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageExpProcAddrTable(
    ze_api_version_t version,
    ze_image_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetMemoryPropertiesExp = zeImageGetMemoryPropertiesExp;
    pDdiTable->pfnViewCreateExp = zeImageViewCreateExp;
    driver_ddiTable.core_ddiTable.ImageExp = *pDdiTable;
    return result;
}
