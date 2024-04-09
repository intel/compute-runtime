/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/experimental/source/tracing/tracing_barrier_imp.h"
#include "level_zero/experimental/source/tracing/tracing_cmdlist_imp.h"
#include "level_zero/experimental/source/tracing/tracing_cmdqueue_imp.h"
#include "level_zero/experimental/source/tracing/tracing_copy_imp.h"
#include "level_zero/experimental/source/tracing/tracing_device_imp.h"
#include "level_zero/experimental/source/tracing/tracing_driver_imp.h"
#include "level_zero/experimental/source/tracing/tracing_event_imp.h"
#include "level_zero/experimental/source/tracing/tracing_fence_imp.h"
#include "level_zero/experimental/source/tracing/tracing_global_imp.h"
#include "level_zero/experimental/source/tracing/tracing_image_imp.h"
#include "level_zero/experimental/source/tracing/tracing_memory_imp.h"
#include "level_zero/experimental/source/tracing/tracing_module_imp.h"
#include "level_zero/experimental/source/tracing/tracing_residency_imp.h"
#include "level_zero/experimental/source/tracing/tracing_sampler_imp.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

#include "ze_core_all_api_entrypoints.h"
#include "ze_ddi_tables.h"

#include <stdlib.h>
#include <string.h>

inline bool getEnvToBool(const char *name) {
    const char *env = getenv(name);
    if ((nullptr == env) || (0 == strcmp("0", env)))
        return false;
    return (0 == strcmp("1", env));
}

ze_gpu_driver_dditable_t driverDdiTable;

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDriverProcAddrTable(
    ze_api_version_t version,
    ze_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::zeDriverGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetApiVersion, L0::zeDriverGetApiVersion, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zeDriverGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIpcProperties, L0::zeDriverGetIpcProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetExtensionProperties, L0::zeDriverGetExtensionProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetExtensionFunctionAddress, L0::zeDriverGetExtensionFunctionAddress, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnGetLastErrorDescription, L0::zeDriverGetLastErrorDescription, version, ZE_API_VERSION_1_6);
    driverDdiTable.coreDdiTable.Driver = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnGet, zeDriverGetTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetApiVersion, zeDriverGetApiVersionTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetProperties, zeDriverGetPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetIpcProperties, zeDriverGetIpcPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetExtensionProperties, zeDriverGetExtensionPropertiesTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetMemProcAddrTable(
    ze_api_version_t version,
    ze_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAllocShared, L0::zeMemAllocShared, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAllocDevice, L0::zeMemAllocDevice, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAllocHost, L0::zeMemAllocHost, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFree, L0::zeMemFree, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAllocProperties, L0::zeMemGetAllocProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAddressRange, L0::zeMemGetAddressRange, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIpcHandle, L0::zeMemGetIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOpenIpcHandle, L0::zeMemOpenIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCloseIpcHandle, L0::zeMemCloseIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFreeExt, L0::zeMemFreeExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnPutIpcHandle, L0::zeMemPutIpcHandle, version, ZE_API_VERSION_1_6);
    driverDdiTable.coreDdiTable.Mem = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnAllocShared, zeMemAllocSharedTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAllocDevice, zeMemAllocDeviceTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAllocHost, zeMemAllocHostTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnFree, zeMemFreeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetAllocProperties, zeMemGetAllocPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetAddressRange, zeMemGetAddressRangeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetIpcHandle, zeMemGetIpcHandleTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnOpenIpcHandle, zeMemOpenIpcHandleTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnCloseIpcHandle, zeMemCloseIpcHandleTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetContextProcAddrTable(
    ze_api_version_t version,
    ze_context_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeContextCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeContextDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetStatus, L0::zeContextGetStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSystemBarrier, L0::zeContextSystemBarrier, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnMakeMemoryResident, L0::zeContextMakeMemoryResident, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEvictMemory, L0::zeContextEvictMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnMakeImageResident, L0::zeContextMakeImageResident, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnEvictImage, L0::zeContextEvictImage, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreateEx, L0::zeContextCreateEx, version, ZE_API_VERSION_1_1);

    driverDdiTable.coreDdiTable.Context = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeContextCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeContextDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetStatus, zeContextGetStatusTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSystemBarrier, zeContextSystemBarrierTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnMakeMemoryResident, zeContextMakeMemoryResidentTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnEvictMemory, zeContextEvictMemoryTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnMakeImageResident, zeContextMakeImageResidentTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnEvictImage, zeContextEvictImageTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetPhysicalMemProcAddrTable(
    ze_api_version_t version,
    ze_physical_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zePhysicalMemCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zePhysicalMemDestroy, version, ZE_API_VERSION_1_0);

    driverDdiTable.coreDdiTable.PhysicalMem = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zePhysicalMemCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zePhysicalMemDestroyTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetVirtualMemProcAddrTable(
    ze_api_version_t version,
    ze_virtual_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnReserve, L0::zeVirtualMemReserve, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnFree, L0::zeVirtualMemFree, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryPageSize, L0::zeVirtualMemQueryPageSize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnMap, L0::zeVirtualMemMap, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnUnmap, L0::zeVirtualMemUnmap, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetAccessAttribute, L0::zeVirtualMemSetAccessAttribute, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAccessAttribute, L0::zeVirtualMemGetAccessAttribute, version, ZE_API_VERSION_1_0);

    driverDdiTable.coreDdiTable.VirtualMem = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnReserve, zeVirtualMemReserveTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnFree, zeVirtualMemFreeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnQueryPageSize, zeVirtualMemQueryPageSizeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnMap, zeVirtualMemMapTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnUnmap, zeVirtualMemUnmapTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSetAccessAttribute, zeVirtualMemSetAccessAttributeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetAccessAttribute, zeVirtualMemGetAccessAttributeTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetGlobalProcAddrTable(
    ze_api_version_t version,
    ze_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnInit, L0::zeInit, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Global = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnInit, zeInitTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDeviceProcAddrTable(
    ze_api_version_t version,
    ze_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::zeDeviceGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCommandQueueGroupProperties, L0::zeDeviceGetCommandQueueGroupProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetSubDevices, L0::zeDeviceGetSubDevices, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zeDeviceGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetComputeProperties, L0::zeDeviceGetComputeProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetModuleProperties, L0::zeDeviceGetModuleProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetMemoryProperties, L0::zeDeviceGetMemoryProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetMemoryAccessProperties, L0::zeDeviceGetMemoryAccessProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetCacheProperties, L0::zeDeviceGetCacheProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetImageProperties, L0::zeDeviceGetImageProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetP2PProperties, L0::zeDeviceGetP2PProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCanAccessPeer, L0::zeDeviceCanAccessPeer, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetStatus, L0::zeDeviceGetStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetExternalMemoryProperties, L0::zeDeviceGetExternalMemoryProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetGlobalTimestamps, L0::zeDeviceGetGlobalTimestamps, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnReserveCacheExt, L0::zeDeviceReserveCacheExt, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnSetCacheAdviceExt, L0::zeDeviceSetCacheAdviceExt, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnPciGetPropertiesExt, L0::zeDevicePciGetPropertiesExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnGetRootDevice, L0::zeDeviceGetRootDevice, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.Device = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnGet, zeDeviceGetTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetCommandQueueGroupProperties, zeDeviceGetCommandQueueGroupPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetSubDevices, zeDeviceGetSubDevicesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetProperties, zeDeviceGetPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetComputeProperties, zeDeviceGetComputePropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetModuleProperties, zeDeviceGetModulePropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetMemoryProperties, zeDeviceGetMemoryPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetMemoryAccessProperties, zeDeviceGetMemoryAccessPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetCacheProperties, zeDeviceGetCachePropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetImageProperties, zeDeviceGetImagePropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetP2PProperties, zeDeviceGetP2PPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnCanAccessPeer, zeDeviceCanAccessPeerTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetStatus, zeDeviceGetStatusTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetExternalMemoryProperties, zeDeviceGetExternalMemoryPropertiesTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDeviceExpProcAddrTable(
    ze_api_version_t version,
    ze_device_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetFabricVertexExp, L0::zeDeviceGetFabricVertexExp, version, ZE_API_VERSION_1_4);
    driverDdiTable.coreDdiTable.DeviceExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandQueueProcAddrTable(
    ze_api_version_t version,
    ze_command_queue_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = L0::zeCommandQueueCreate;
    pDdiTable->pfnDestroy = L0::zeCommandQueueDestroy;
    pDdiTable->pfnExecuteCommandLists = L0::zeCommandQueueExecuteCommandLists;
    pDdiTable->pfnSynchronize = L0::zeCommandQueueSynchronize;
    fillDdiEntry(pDdiTable->pfnGetOrdinal, L0::zeCommandQueueGetOrdinal, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetIndex, L0::zeCommandQueueGetIndex, version, ZE_API_VERSION_1_9);

    driverDdiTable.coreDdiTable.CommandQueue = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
    if (version >= ZE_API_VERSION_1_3) {
        pDdiTable->pfnAppendImageCopyToMemoryExt = L0::zeCommandListAppendImageCopyToMemoryExt;
        pDdiTable->pfnAppendImageCopyFromMemoryExt = L0::zeCommandListAppendImageCopyFromMemoryExt;
    }
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
    pDdiTable->pfnHostSynchronize = L0::zeCommandListHostSynchronize;
    if (version >= ZE_API_VERSION_1_9) {
        pDdiTable->pfnGetDeviceHandle = L0::zeCommandListGetDeviceHandle;
        pDdiTable->pfnGetContextHandle = L0::zeCommandListGetContextHandle;
        pDdiTable->pfnGetOrdinal = L0::zeCommandListGetOrdinal;
        pDdiTable->pfnImmediateGetIndex = L0::zeCommandListImmediateGetIndex;
        pDdiTable->pfnIsImmediate = L0::zeCommandListIsImmediate;
    }
    driverDdiTable.coreDdiTable.CommandList = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = L0::zeFenceCreate;
    pDdiTable->pfnDestroy = L0::zeFenceDestroy;
    pDdiTable->pfnHostSynchronize = L0::zeFenceHostSynchronize;
    pDdiTable->pfnQueryStatus = L0::zeFenceQueryStatus;
    pDdiTable->pfnReset = L0::zeFenceReset;
    driverDdiTable.coreDdiTable.Fence = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = L0::zeEventPoolCreate;
    pDdiTable->pfnDestroy = L0::zeEventPoolDestroy;
    pDdiTable->pfnGetIpcHandle = L0::zeEventPoolGetIpcHandle;
    pDdiTable->pfnOpenIpcHandle = L0::zeEventPoolOpenIpcHandle;
    pDdiTable->pfnCloseIpcHandle = L0::zeEventPoolCloseIpcHandle;
    if (version >= ZE_API_VERSION_1_9) {
        pDdiTable->pfnGetContextHandle = L0::zeEventPoolGetContextHandle;
        pDdiTable->pfnGetFlags = L0::zeEventPoolGetFlags;
    }
    driverDdiTable.coreDdiTable.EventPool = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = L0::zeEventCreate;
    pDdiTable->pfnDestroy = L0::zeEventDestroy;
    pDdiTable->pfnHostSignal = L0::zeEventHostSignal;
    pDdiTable->pfnHostSynchronize = L0::zeEventHostSynchronize;
    pDdiTable->pfnQueryStatus = L0::zeEventQueryStatus;
    pDdiTable->pfnHostReset = L0::zeEventHostReset;
    pDdiTable->pfnQueryKernelTimestamp = L0::zeEventQueryKernelTimestamp;
    pDdiTable->pfnQueryKernelTimestampsExt = L0::zeEventQueryKernelTimestampsExt;
    if (version >= ZE_API_VERSION_1_9) {
        pDdiTable->pfnGetEventPool = L0::zeEventGetEventPool;
        pDdiTable->pfnGetSignalScope = L0::zeEventGetSignalScope;
        pDdiTable->pfnGetWaitScope = L0::zeEventGetWaitScope;
    }
    driverDdiTable.coreDdiTable.Event = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = L0::zeImageGetProperties;
    pDdiTable->pfnCreate = L0::zeImageCreate;
    pDdiTable->pfnDestroy = L0::zeImageDestroy;
    pDdiTable->pfnGetAllocPropertiesExt = L0::zeImageGetAllocPropertiesExt;
    pDdiTable->pfnViewCreateExt = L0::zeImageViewCreateExt;
    driverDdiTable.coreDdiTable.Image = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = L0::zeModuleCreate;
    pDdiTable->pfnDestroy = L0::zeModuleDestroy;
    pDdiTable->pfnDynamicLink = L0::zeModuleDynamicLink;
    pDdiTable->pfnGetNativeBinary = L0::zeModuleGetNativeBinary;
    pDdiTable->pfnGetGlobalPointer = L0::zeModuleGetGlobalPointer;
    pDdiTable->pfnGetKernelNames = L0::zeModuleGetKernelNames;
    pDdiTable->pfnGetFunctionPointer = L0::zeModuleGetFunctionPointer;
    pDdiTable->pfnGetProperties = L0::zeModuleGetProperties;
    pDdiTable->pfnInspectLinkageExt = L0::zeModuleInspectLinkageExt;
    driverDdiTable.coreDdiTable.Module = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnDestroy = L0::zeModuleBuildLogDestroy;
    pDdiTable->pfnGetString = L0::zeModuleBuildLogGetString;
    driverDdiTable.coreDdiTable.ModuleBuildLog = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
    driverDdiTable.coreDdiTable.Kernel = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = L0::zeSamplerCreate;
    pDdiTable->pfnDestroy = L0::zeSamplerDestroy;
    driverDdiTable.coreDdiTable.Sampler = *pDdiTable;
    if (driverDdiTable.enableTracing) {
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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnSetGlobalOffsetExp = L0::zeKernelSetGlobalOffsetExp;
    pDdiTable->pfnSchedulingHintExp = L0::zeKernelSchedulingHintExp;
    driverDdiTable.coreDdiTable.KernelExp = *pDdiTable;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetMemExpProcAddrTable(
    ze_api_version_t version,
    ze_mem_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetIpcHandleFromFileDescriptorExp = L0::zeMemGetIpcHandleFromFileDescriptorExp;
    pDdiTable->pfnGetFileDescriptorFromIpcHandleExp = L0::zeMemGetFileDescriptorFromIpcHandleExp;
    pDdiTable->pfnSetAtomicAccessAttributeExp = L0::zeMemSetAtomicAccessAttributeExp;
    pDdiTable->pfnGetAtomicAccessAttributeExp = L0::zeMemGetAtomicAccessAttributeExp;
    driverDdiTable.coreDdiTable.MemExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageExpProcAddrTable(
    ze_api_version_t version,
    ze_image_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetMemoryPropertiesExp = L0::zeImageGetMemoryPropertiesExp;
    pDdiTable->pfnViewCreateExp = L0::zeImageViewCreateExp;
    driverDdiTable.coreDdiTable.ImageExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFabricVertexExpProcAddrTable(
    ze_api_version_t version,
    ze_fabric_vertex_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetExp = L0::zeFabricVertexGetExp;
    pDdiTable->pfnGetSubVerticesExp = L0::zeFabricVertexGetSubVerticesExp;
    pDdiTable->pfnGetPropertiesExp = L0::zeFabricVertexGetPropertiesExp;
    pDdiTable->pfnGetDeviceExp = L0::zeFabricVertexGetDeviceExp;
    driverDdiTable.coreDdiTable.FabricVertexExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFabricEdgeExpProcAddrTable(
    ze_api_version_t version,
    ze_fabric_edge_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetExp = L0::zeFabricEdgeGetExp;
    pDdiTable->pfnGetVerticesExp = L0::zeFabricEdgeGetVerticesExp;
    pDdiTable->pfnGetPropertiesExp = L0::zeFabricEdgeGetPropertiesExp;
    driverDdiTable.coreDdiTable.FabricEdgeExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDriverExpProcAddrTable(
    ze_api_version_t version,
    ze_driver_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnRTASFormatCompatibilityCheckExp = L0::zeDriverRTASFormatCompatibilityCheckExp;
    driverDdiTable.coreDdiTable.DriverExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASParallelOperationExpProcAddrTable(
    ze_api_version_t version,
    ze_rtas_parallel_operation_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreateExp = L0::zeRTASParallelOperationCreateExp;
    pDdiTable->pfnGetPropertiesExp = L0::zeRTASParallelOperationGetPropertiesExp;
    pDdiTable->pfnJoinExp = L0::zeRTASParallelOperationJoinExp;
    pDdiTable->pfnDestroyExp = L0::zeRTASParallelOperationDestroyExp;
    driverDdiTable.coreDdiTable.RTASParallelOperationExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASBuilderExpProcAddrTable(
    ze_api_version_t version,
    ze_rtas_builder_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreateExp = L0::zeRTASBuilderCreateExp;
    pDdiTable->pfnGetBuildPropertiesExp = L0::zeRTASBuilderGetBuildPropertiesExp;
    pDdiTable->pfnBuildExp = L0::zeRTASBuilderBuildExp;
    pDdiTable->pfnDestroyExp = L0::zeRTASBuilderDestroyExp;
    driverDdiTable.coreDdiTable.RTASBuilderExp = *pDdiTable;
    return result;
}
