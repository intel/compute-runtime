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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = zeDriverGet;
    pDdiTable->pfnGetApiVersion = zeDriverGetApiVersion;
    pDdiTable->pfnGetProperties = zeDriverGetProperties;
    pDdiTable->pfnGetIpcProperties = zeDriverGetIpcProperties;
    pDdiTable->pfnGetExtensionProperties = zeDriverGetExtensionProperties;
    pDdiTable->pfnGetExtensionFunctionAddress = zeDriverGetExtensionFunctionAddress;
    driver_ddiTable.core_ddiTable.Driver = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGet = zeDriverGetTracing;
        pDdiTable->pfnGetApiVersion = zeDriverGetApiVersionTracing;
        pDdiTable->pfnGetProperties = zeDriverGetPropertiesTracing;
        pDdiTable->pfnGetIpcProperties = zeDriverGetIpcPropertiesTracing;
        pDdiTable->pfnGetExtensionProperties = zeDriverGetExtensionPropertiesTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnAllocShared = zeMemAllocSharedTracing;
        pDdiTable->pfnAllocDevice = zeMemAllocDeviceTracing;
        pDdiTable->pfnAllocHost = zeMemAllocHostTracing;
        pDdiTable->pfnFree = zeMemFreeTracing;
        pDdiTable->pfnGetAllocProperties = zeMemGetAllocPropertiesTracing;
        pDdiTable->pfnGetAddressRange = zeMemGetAddressRangeTracing;
        pDdiTable->pfnGetIpcHandle = zeMemGetIpcHandleTracing;
        pDdiTable->pfnOpenIpcHandle = zeMemOpenIpcHandleTracing;
        pDdiTable->pfnCloseIpcHandle = zeMemCloseIpcHandleTracing;
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

    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnCreate = zeContextCreateTracing;
        pDdiTable->pfnDestroy = zeContextDestroyTracing;
        pDdiTable->pfnGetStatus = zeContextGetStatusTracing;
        pDdiTable->pfnSystemBarrier = zeContextSystemBarrierTracing;
        pDdiTable->pfnMakeMemoryResident = zeContextMakeMemoryResidentTracing;
        pDdiTable->pfnEvictMemory = zeContextEvictMemoryTracing;
        pDdiTable->pfnMakeImageResident = zeContextMakeImageResidentTracing;
        pDdiTable->pfnEvictImage = zeContextEvictImageTracing;
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

    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zePhysicalMemCreate;
    pDdiTable->pfnDestroy = zePhysicalMemDestroy;

    driver_ddiTable.core_ddiTable.PhysicalMem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zePhysicalMemCreateTracing;
        pDdiTable->pfnDestroy = zePhysicalMemDestroyTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnReserve = zeVirtualMemReserveTracing;
        pDdiTable->pfnFree = zeVirtualMemFreeTracing;
        pDdiTable->pfnQueryPageSize = zeVirtualMemQueryPageSizeTracing;
        pDdiTable->pfnMap = zeVirtualMemMapTracing;
        pDdiTable->pfnUnmap = zeVirtualMemUnmapTracing;
        pDdiTable->pfnSetAccessAttribute = zeVirtualMemSetAccessAttributeTracing;
        pDdiTable->pfnGetAccessAttribute = zeVirtualMemGetAccessAttributeTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnInit = zeInit;
    driver_ddiTable.core_ddiTable.Global = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnInit = zeInitTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnGet = zeDeviceGetTracing;
        pDdiTable->pfnGetCommandQueueGroupProperties = zeDeviceGetCommandQueueGroupPropertiesTracing;
        pDdiTable->pfnGetSubDevices = zeDeviceGetSubDevicesTracing;
        pDdiTable->pfnGetProperties = zeDeviceGetPropertiesTracing;
        pDdiTable->pfnGetComputeProperties = zeDeviceGetComputePropertiesTracing;
        pDdiTable->pfnGetModuleProperties = zeDeviceGetModulePropertiesTracing;
        pDdiTable->pfnGetMemoryProperties = zeDeviceGetMemoryPropertiesTracing;
        pDdiTable->pfnGetMemoryAccessProperties = zeDeviceGetMemoryAccessPropertiesTracing;
        pDdiTable->pfnGetCacheProperties = zeDeviceGetCachePropertiesTracing;
        pDdiTable->pfnGetImageProperties = zeDeviceGetImagePropertiesTracing;
        pDdiTable->pfnGetP2PProperties = zeDeviceGetP2PPropertiesTracing;
        pDdiTable->pfnCanAccessPeer = zeDeviceCanAccessPeerTracing;
        pDdiTable->pfnGetStatus = zeDeviceGetStatusTracing;
        pDdiTable->pfnGetExternalMemoryProperties = zeDeviceGetExternalMemoryPropertiesTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeCommandQueueCreate;
    pDdiTable->pfnDestroy = zeCommandQueueDestroy;
    pDdiTable->pfnExecuteCommandLists = zeCommandQueueExecuteCommandLists;
    pDdiTable->pfnSynchronize = zeCommandQueueSynchronize;
    driver_ddiTable.core_ddiTable.CommandQueue = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeCommandQueueCreateTracing;
        pDdiTable->pfnDestroy = zeCommandQueueDestroyTracing;
        pDdiTable->pfnExecuteCommandLists = zeCommandQueueExecuteCommandListsTracing;
        pDdiTable->pfnSynchronize = zeCommandQueueSynchronizeTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnAppendBarrier = zeCommandListAppendBarrierTracing;
        pDdiTable->pfnAppendMemoryRangesBarrier = zeCommandListAppendMemoryRangesBarrierTracing;
        pDdiTable->pfnCreate = zeCommandListCreateTracing;
        pDdiTable->pfnCreateImmediate = zeCommandListCreateImmediateTracing;
        pDdiTable->pfnDestroy = zeCommandListDestroyTracing;
        pDdiTable->pfnClose = zeCommandListCloseTracing;
        pDdiTable->pfnReset = zeCommandListResetTracing;
        pDdiTable->pfnAppendMemoryCopy = zeCommandListAppendMemoryCopyTracing;
        pDdiTable->pfnAppendMemoryCopyRegion = zeCommandListAppendMemoryCopyRegionTracing;
        pDdiTable->pfnAppendMemoryFill = zeCommandListAppendMemoryFillTracing;
        pDdiTable->pfnAppendImageCopy = zeCommandListAppendImageCopyTracing;
        pDdiTable->pfnAppendImageCopyRegion = zeCommandListAppendImageCopyRegionTracing;
        pDdiTable->pfnAppendImageCopyToMemory = zeCommandListAppendImageCopyToMemoryTracing;
        pDdiTable->pfnAppendImageCopyFromMemory = zeCommandListAppendImageCopyFromMemoryTracing;
        pDdiTable->pfnAppendMemoryPrefetch = zeCommandListAppendMemoryPrefetchTracing;
        pDdiTable->pfnAppendMemAdvise = zeCommandListAppendMemAdviseTracing;
        pDdiTable->pfnAppendSignalEvent = zeCommandListAppendSignalEventTracing;
        pDdiTable->pfnAppendWaitOnEvents = zeCommandListAppendWaitOnEventsTracing;
        pDdiTable->pfnAppendEventReset = zeCommandListAppendEventResetTracing;
        pDdiTable->pfnAppendLaunchKernel = zeCommandListAppendLaunchKernelTracing;
        pDdiTable->pfnAppendLaunchCooperativeKernel = zeCommandListAppendLaunchCooperativeKernelTracing;
        pDdiTable->pfnAppendLaunchKernelIndirect = zeCommandListAppendLaunchKernelIndirectTracing;
        pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = zeCommandListAppendLaunchMultipleKernelsIndirectTracing;
        pDdiTable->pfnAppendWriteGlobalTimestamp = zeCommandListAppendWriteGlobalTimestampTracing;
        pDdiTable->pfnAppendMemoryCopyFromContext = zeCommandListAppendMemoryCopyFromContextTracing;
        pDdiTable->pfnAppendQueryKernelTimestamps = zeCommandListAppendQueryKernelTimestampsTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeFenceCreate;
    pDdiTable->pfnDestroy = zeFenceDestroy;
    pDdiTable->pfnHostSynchronize = zeFenceHostSynchronize;
    pDdiTable->pfnQueryStatus = zeFenceQueryStatus;
    pDdiTable->pfnReset = zeFenceReset;
    driver_ddiTable.core_ddiTable.Fence = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeFenceCreateTracing;
        pDdiTable->pfnDestroy = zeFenceDestroyTracing;
        pDdiTable->pfnHostSynchronize = zeFenceHostSynchronizeTracing;
        pDdiTable->pfnQueryStatus = zeFenceQueryStatusTracing;
        pDdiTable->pfnReset = zeFenceResetTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeEventPoolCreate;
    pDdiTable->pfnDestroy = zeEventPoolDestroy;
    pDdiTable->pfnGetIpcHandle = zeEventPoolGetIpcHandle;
    pDdiTable->pfnOpenIpcHandle = zeEventPoolOpenIpcHandle;
    pDdiTable->pfnCloseIpcHandle = zeEventPoolCloseIpcHandle;
    driver_ddiTable.core_ddiTable.EventPool = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeEventPoolCreateTracing;
        pDdiTable->pfnDestroy = zeEventPoolDestroyTracing;
        pDdiTable->pfnGetIpcHandle = zeEventPoolGetIpcHandleTracing;
        pDdiTable->pfnOpenIpcHandle = zeEventPoolOpenIpcHandleTracing;
        pDdiTable->pfnCloseIpcHandle = zeEventPoolCloseIpcHandleTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnCreate = zeEventCreateTracing;
        pDdiTable->pfnDestroy = zeEventDestroyTracing;
        pDdiTable->pfnHostSignal = zeEventHostSignalTracing;
        pDdiTable->pfnHostSynchronize = zeEventHostSynchronizeTracing;
        pDdiTable->pfnQueryStatus = zeEventQueryStatusTracing;
        pDdiTable->pfnHostReset = zeEventHostResetTracing;
        pDdiTable->pfnQueryKernelTimestamp = zeEventQueryKernelTimestampTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = zeImageGetProperties;
    pDdiTable->pfnCreate = zeImageCreate;
    pDdiTable->pfnDestroy = zeImageDestroy;
    pDdiTable->pfnGetAllocPropertiesExt = zeImageGetAllocPropertiesExt;
    driver_ddiTable.core_ddiTable.Image = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGetProperties = zeImageGetPropertiesTracing;
        pDdiTable->pfnCreate = zeImageCreateTracing;
        pDdiTable->pfnDestroy = zeImageDestroyTracing;
        pDdiTable->pfnGetAllocPropertiesExt = zeImageGetAllocPropertiesExt;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnCreate = zeModuleCreateTracing;
        pDdiTable->pfnDestroy = zeModuleDestroyTracing;
        pDdiTable->pfnGetNativeBinary = zeModuleGetNativeBinaryTracing;
        pDdiTable->pfnDynamicLink = zeModuleDynamicLinkTracing;
        pDdiTable->pfnGetGlobalPointer = zeModuleGetGlobalPointerTracing;
        pDdiTable->pfnGetFunctionPointer = zeModuleGetFunctionPointerTracing;
        pDdiTable->pfnGetKernelNames = zeModuleGetKernelNamesTracing;
        pDdiTable->pfnGetProperties = zeModuleGetPropertiesTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnDestroy = zeModuleBuildLogDestroy;
    pDdiTable->pfnGetString = zeModuleBuildLogGetString;
    driver_ddiTable.core_ddiTable.ModuleBuildLog = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnDestroy = zeModuleBuildLogDestroyTracing;
        pDdiTable->pfnGetString = zeModuleBuildLogGetStringTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
        pDdiTable->pfnCreate = zeKernelCreateTracing;
        pDdiTable->pfnDestroy = zeKernelDestroyTracing;
        pDdiTable->pfnSetGroupSize = zeKernelSetGroupSizeTracing;
        pDdiTable->pfnSuggestGroupSize = zeKernelSuggestGroupSizeTracing;
        pDdiTable->pfnSuggestMaxCooperativeGroupCount = zeKernelSuggestMaxCooperativeGroupCountTracing;
        pDdiTable->pfnSetArgumentValue = zeKernelSetArgumentValueTracing;
        pDdiTable->pfnSetIndirectAccess = zeKernelSetIndirectAccessTracing;
        pDdiTable->pfnGetIndirectAccess = zeKernelGetIndirectAccessTracing;
        pDdiTable->pfnGetSourceAttributes = zeKernelGetSourceAttributesTracing;
        pDdiTable->pfnGetProperties = zeKernelGetPropertiesTracing;
        pDdiTable->pfnSetCacheConfig = zeKernelSetCacheConfigTracing;
        pDdiTable->pfnGetName = zeKernelGetNameTracing;
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
    driver_ddiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeSamplerCreate;
    pDdiTable->pfnDestroy = zeSamplerDestroy;
    driver_ddiTable.core_ddiTable.Sampler = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeSamplerCreateTracing;
        pDdiTable->pfnDestroy = zeSamplerDestroyTracing;
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
