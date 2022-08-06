/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/experimental/source/tracing/tracing_imp.h"
#include "level_zero/source/inc/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_core_all_api_entrypoints.h"
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
    pDdiTable->pfnGet = L0::zeDriverGet;
    pDdiTable->pfnGetApiVersion = L0::zeDriverGetApiVersion;
    pDdiTable->pfnGetProperties = L0::zeDriverGetProperties;
    pDdiTable->pfnGetIpcProperties = L0::zeDriverGetIpcProperties;
    pDdiTable->pfnGetExtensionProperties = L0::zeDriverGetExtensionProperties;
    pDdiTable->pfnGetExtensionFunctionAddress = L0::zeDriverGetExtensionFunctionAddress;
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
    pDdiTable->pfnAllocShared = L0::zeMemAllocShared;
    pDdiTable->pfnAllocDevice = L0::zeMemAllocDevice;
    pDdiTable->pfnAllocHost = L0::zeMemAllocHost;
    pDdiTable->pfnFree = L0::zeMemFree;
    pDdiTable->pfnFreeExt = L0::zeMemFreeExt;
    pDdiTable->pfnGetAllocProperties = L0::zeMemGetAllocProperties;
    pDdiTable->pfnGetAddressRange = L0::zeMemGetAddressRange;
    pDdiTable->pfnGetIpcHandle = L0::zeMemGetIpcHandle;
    pDdiTable->pfnOpenIpcHandle = L0::zeMemOpenIpcHandle;
    pDdiTable->pfnCloseIpcHandle = L0::zeMemCloseIpcHandle;
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
    pDdiTable->pfnCreate = L0::zeContextCreate;
    pDdiTable->pfnCreateEx = L0::zeContextCreateEx;
    pDdiTable->pfnDestroy = L0::zeContextDestroy;
    pDdiTable->pfnGetStatus = L0::zeContextGetStatus;
    pDdiTable->pfnSystemBarrier = L0::zeContextSystemBarrier;
    pDdiTable->pfnMakeMemoryResident = L0::zeContextMakeMemoryResident;
    pDdiTable->pfnEvictMemory = L0::zeContextEvictMemory;
    pDdiTable->pfnMakeImageResident = L0::zeContextMakeImageResident;
    pDdiTable->pfnEvictImage = L0::zeContextEvictImage;

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
    pDdiTable->pfnCreate = L0::zePhysicalMemCreate;
    pDdiTable->pfnDestroy = L0::zePhysicalMemDestroy;

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
    pDdiTable->pfnReserve = L0::zeVirtualMemReserve;
    pDdiTable->pfnFree = L0::zeVirtualMemFree;
    pDdiTable->pfnQueryPageSize = L0::zeVirtualMemQueryPageSize;
    pDdiTable->pfnMap = L0::zeVirtualMemMap;
    pDdiTable->pfnUnmap = L0::zeVirtualMemUnmap;
    pDdiTable->pfnSetAccessAttribute = L0::zeVirtualMemSetAccessAttribute;
    pDdiTable->pfnGetAccessAttribute = L0::zeVirtualMemGetAccessAttribute;

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
    pDdiTable->pfnInit = L0::zeInit;
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
    pDdiTable->pfnGet = L0::zeDeviceGet;
    pDdiTable->pfnGetCommandQueueGroupProperties = L0::zeDeviceGetCommandQueueGroupProperties;
    pDdiTable->pfnGetSubDevices = L0::zeDeviceGetSubDevices;
    pDdiTable->pfnGetProperties = L0::zeDeviceGetProperties;
    pDdiTable->pfnGetComputeProperties = L0::zeDeviceGetComputeProperties;
    pDdiTable->pfnGetModuleProperties = L0::zeDeviceGetModuleProperties;
    pDdiTable->pfnGetMemoryProperties = L0::zeDeviceGetMemoryProperties;
    pDdiTable->pfnGetMemoryAccessProperties = L0::zeDeviceGetMemoryAccessProperties;
    pDdiTable->pfnGetCacheProperties = L0::zeDeviceGetCacheProperties;
    pDdiTable->pfnGetImageProperties = L0::zeDeviceGetImageProperties;
    pDdiTable->pfnGetP2PProperties = L0::zeDeviceGetP2PProperties;
    pDdiTable->pfnCanAccessPeer = L0::zeDeviceCanAccessPeer;
    pDdiTable->pfnGetStatus = L0::zeDeviceGetStatus;
    pDdiTable->pfnGetExternalMemoryProperties = L0::zeDeviceGetExternalMemoryProperties;
    pDdiTable->pfnGetGlobalTimestamps = L0::zeDeviceGetGlobalTimestamps;
    pDdiTable->pfnReserveCacheExt = L0::zeDeviceReserveCacheExt;
    pDdiTable->pfnSetCacheAdviceExt = L0::zeDeviceSetCacheAdviceExt;
    pDdiTable->pfnPciGetPropertiesExt = L0::zeDevicePciGetPropertiesExt;
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
    pDdiTable->pfnCreate = L0::zeCommandQueueCreate;
    pDdiTable->pfnDestroy = L0::zeCommandQueueDestroy;
    pDdiTable->pfnExecuteCommandLists = L0::zeCommandQueueExecuteCommandLists;
    pDdiTable->pfnSynchronize = L0::zeCommandQueueSynchronize;
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
    pDdiTable->pfnAppendBarrier = L0::zeCommandListAppendBarrier;
    pDdiTable->pfnAppendMemoryRangesBarrier = L0::zeCommandListAppendMemoryRangesBarrier;
    pDdiTable->pfnCreate = L0::zeCommandListCreate;
    pDdiTable->pfnCreateImmediate = L0::zeCommandListCreateImmediate;
    pDdiTable->pfnDestroy = L0::zeCommandListDestroy;
    pDdiTable->pfnClose = L0::zeCommandListClose;
    pDdiTable->pfnReset = L0::zeCommandListReset;
    pDdiTable->pfnAppendMemoryCopy = L0::zeCommandListAppendMemoryCopy;
    pDdiTable->pfnAppendMemoryCopyRegion = L0::zeCommandListAppendMemoryCopyRegion;
    pDdiTable->pfnAppendMemoryFill = L0::zeCommandListAppendMemoryFill;
    pDdiTable->pfnAppendImageCopy = L0::zeCommandListAppendImageCopy;
    pDdiTable->pfnAppendImageCopyRegion = L0::zeCommandListAppendImageCopyRegion;
    pDdiTable->pfnAppendImageCopyToMemory = L0::zeCommandListAppendImageCopyToMemory;
    pDdiTable->pfnAppendImageCopyFromMemory = L0::zeCommandListAppendImageCopyFromMemory;
    pDdiTable->pfnAppendMemoryPrefetch = L0::zeCommandListAppendMemoryPrefetch;
    pDdiTable->pfnAppendMemAdvise = L0::zeCommandListAppendMemAdvise;
    pDdiTable->pfnAppendSignalEvent = L0::zeCommandListAppendSignalEvent;
    pDdiTable->pfnAppendWaitOnEvents = L0::zeCommandListAppendWaitOnEvents;
    pDdiTable->pfnAppendEventReset = L0::zeCommandListAppendEventReset;
    pDdiTable->pfnAppendLaunchKernel = L0::zeCommandListAppendLaunchKernel;
    pDdiTable->pfnAppendLaunchCooperativeKernel = L0::zeCommandListAppendLaunchCooperativeKernel;
    pDdiTable->pfnAppendLaunchKernelIndirect = L0::zeCommandListAppendLaunchKernelIndirect;
    pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = L0::zeCommandListAppendLaunchMultipleKernelsIndirect;
    pDdiTable->pfnAppendWriteGlobalTimestamp = L0::zeCommandListAppendWriteGlobalTimestamp;
    pDdiTable->pfnAppendMemoryCopyFromContext = L0::zeCommandListAppendMemoryCopyFromContext;
    pDdiTable->pfnAppendQueryKernelTimestamps = L0::zeCommandListAppendQueryKernelTimestamps;
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
    pDdiTable->pfnCreate = L0::zeFenceCreate;
    pDdiTable->pfnDestroy = L0::zeFenceDestroy;
    pDdiTable->pfnHostSynchronize = L0::zeFenceHostSynchronize;
    pDdiTable->pfnQueryStatus = L0::zeFenceQueryStatus;
    pDdiTable->pfnReset = L0::zeFenceReset;
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
    pDdiTable->pfnCreate = L0::zeEventPoolCreate;
    pDdiTable->pfnDestroy = L0::zeEventPoolDestroy;
    pDdiTable->pfnGetIpcHandle = L0::zeEventPoolGetIpcHandle;
    pDdiTable->pfnOpenIpcHandle = L0::zeEventPoolOpenIpcHandle;
    pDdiTable->pfnCloseIpcHandle = L0::zeEventPoolCloseIpcHandle;
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
    pDdiTable->pfnCreate = L0::zeEventCreate;
    pDdiTable->pfnDestroy = L0::zeEventDestroy;
    pDdiTable->pfnHostSignal = L0::zeEventHostSignal;
    pDdiTable->pfnHostSynchronize = L0::zeEventHostSynchronize;
    pDdiTable->pfnQueryStatus = L0::zeEventQueryStatus;
    pDdiTable->pfnHostReset = L0::zeEventHostReset;
    pDdiTable->pfnQueryKernelTimestamp = L0::zeEventQueryKernelTimestamp;
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
    pDdiTable->pfnQueryTimestampsExp = L0::zeEventQueryTimestampsExp;

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
    pDdiTable->pfnGetProperties = L0::zeImageGetProperties;
    pDdiTable->pfnCreate = L0::zeImageCreate;
    pDdiTable->pfnDestroy = L0::zeImageDestroy;
    pDdiTable->pfnGetAllocPropertiesExt = L0::zeImageGetAllocPropertiesExt;
    driver_ddiTable.core_ddiTable.Image = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGetProperties = zeImageGetPropertiesTracing;
        pDdiTable->pfnCreate = zeImageCreateTracing;
        pDdiTable->pfnDestroy = zeImageDestroyTracing;
        pDdiTable->pfnGetAllocPropertiesExt = L0::zeImageGetAllocPropertiesExt;
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
    pDdiTable->pfnCreate = L0::zeModuleCreate;
    pDdiTable->pfnDestroy = L0::zeModuleDestroy;
    pDdiTable->pfnDynamicLink = L0::zeModuleDynamicLink;
    pDdiTable->pfnGetNativeBinary = L0::zeModuleGetNativeBinary;
    pDdiTable->pfnGetGlobalPointer = L0::zeModuleGetGlobalPointer;
    pDdiTable->pfnGetKernelNames = L0::zeModuleGetKernelNames;
    pDdiTable->pfnGetFunctionPointer = L0::zeModuleGetFunctionPointer;
    pDdiTable->pfnGetProperties = L0::zeModuleGetProperties;
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
    pDdiTable->pfnDestroy = L0::zeModuleBuildLogDestroy;
    pDdiTable->pfnGetString = L0::zeModuleBuildLogGetString;
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
    pDdiTable->pfnCreate = L0::zeKernelCreate;
    pDdiTable->pfnDestroy = L0::zeKernelDestroy;
    pDdiTable->pfnSetGroupSize = L0::zeKernelSetGroupSize;
    pDdiTable->pfnSuggestGroupSize = L0::zeKernelSuggestGroupSize;
    pDdiTable->pfnSuggestMaxCooperativeGroupCount = L0::zeKernelSuggestMaxCooperativeGroupCount;
    pDdiTable->pfnSetArgumentValue = L0::zeKernelSetArgumentValue;
    pDdiTable->pfnSetIndirectAccess = L0::zeKernelSetIndirectAccess;
    pDdiTable->pfnGetIndirectAccess = L0::zeKernelGetIndirectAccess;
    pDdiTable->pfnGetSourceAttributes = L0::zeKernelGetSourceAttributes;
    pDdiTable->pfnGetProperties = L0::zeKernelGetProperties;
    pDdiTable->pfnSetCacheConfig = L0::zeKernelSetCacheConfig;
    pDdiTable->pfnGetName = L0::zeKernelGetName;
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
    pDdiTable->pfnCreate = L0::zeSamplerCreate;
    pDdiTable->pfnDestroy = L0::zeSamplerDestroy;
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
    pDdiTable->pfnSetGlobalOffsetExp = L0::zeKernelSetGlobalOffsetExp;
    pDdiTable->pfnSchedulingHintExp = L0::zeKernelSchedulingHintExp;
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
    pDdiTable->pfnGetMemoryPropertiesExp = L0::zeImageGetMemoryPropertiesExp;
    pDdiTable->pfnViewCreateExp = L0::zeImageViewCreateExp;
    driver_ddiTable.core_ddiTable.ImageExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFabricVertexExpProcAddrTable(
    ze_api_version_t version,
    ze_fabric_vertex_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetExp = L0::zeFabricVertexGetExp;
    pDdiTable->pfnGetSubVerticesExp = L0::zeFabricVertexGetSubVerticesExp;
    pDdiTable->pfnGetPropertiesExp = L0::zeFabricVertexGetPropertiesExp;
    pDdiTable->pfnGetDeviceExp = L0::zeFabricVertexGetDeviceExp;
    driver_ddiTable.core_ddiTable.FabricVertexExp = *pDdiTable;
    return result;
}
