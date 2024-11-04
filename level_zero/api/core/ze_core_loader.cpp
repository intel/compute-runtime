/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/io_functions.h"

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

ze_gpu_driver_dditable_t driverDdiTable;

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDriverProcAddrTable(
    ze_api_version_t version,
    ze_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");
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
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
    fillDdiEntry(pDdiTable->pfnGetPitchFor2dImage, L0::zeMemGetPitchFor2dImage, version, ZE_API_VERSION_1_9);
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

    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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

    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnInit, L0::zeInit, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Global = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnInit, zeInitTracing, version, ZE_API_VERSION_1_0);
    }
    fillDdiEntry(pDdiTable->pfnInitDrivers, L0::zeInitDrivers, version, ZE_API_VERSION_1_10);
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
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

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
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeCommandQueueCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeCommandQueueDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnExecuteCommandLists, L0::zeCommandQueueExecuteCommandLists, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSynchronize, L0::zeCommandQueueSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetOrdinal, L0::zeCommandQueueGetOrdinal, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetIndex, L0::zeCommandQueueGetIndex, version, ZE_API_VERSION_1_9);

    driverDdiTable.coreDdiTable.CommandQueue = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeCommandQueueCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeCommandQueueDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnExecuteCommandLists, zeCommandQueueExecuteCommandListsTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSynchronize, zeCommandQueueSynchronizeTracing, version, ZE_API_VERSION_1_0);
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
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAppendBarrier, L0::zeCommandListAppendBarrier, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryRangesBarrier, L0::zeCommandListAppendMemoryRangesBarrier, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeCommandListCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreateImmediate, L0::zeCommandListCreateImmediate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeCommandListDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnClose, L0::zeCommandListClose, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::zeCommandListReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryCopy, L0::zeCommandListAppendMemoryCopy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryCopyRegion, L0::zeCommandListAppendMemoryCopyRegion, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryFill, L0::zeCommandListAppendMemoryFill, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopy, L0::zeCommandListAppendImageCopy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyRegion, L0::zeCommandListAppendImageCopyRegion, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyToMemory, L0::zeCommandListAppendImageCopyToMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyFromMemory, L0::zeCommandListAppendImageCopyFromMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryPrefetch, L0::zeCommandListAppendMemoryPrefetch, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemAdvise, L0::zeCommandListAppendMemAdvise, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendSignalEvent, L0::zeCommandListAppendSignalEvent, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendWaitOnEvents, L0::zeCommandListAppendWaitOnEvents, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendEventReset, L0::zeCommandListAppendEventReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchKernel, L0::zeCommandListAppendLaunchKernel, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchCooperativeKernel, L0::zeCommandListAppendLaunchCooperativeKernel, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchKernelIndirect, L0::zeCommandListAppendLaunchKernelIndirect, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendLaunchMultipleKernelsIndirect, L0::zeCommandListAppendLaunchMultipleKernelsIndirect, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendWriteGlobalTimestamp, L0::zeCommandListAppendWriteGlobalTimestamp, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMemoryCopyFromContext, L0::zeCommandListAppendMemoryCopyFromContext, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendQueryKernelTimestamps, L0::zeCommandListAppendQueryKernelTimestamps, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSynchronize, L0::zeCommandListHostSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyToMemoryExt, L0::zeCommandListAppendImageCopyToMemoryExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnAppendImageCopyFromMemoryExt, L0::zeCommandListAppendImageCopyFromMemoryExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnGetDeviceHandle, L0::zeCommandListGetDeviceHandle, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetContextHandle, L0::zeCommandListGetContextHandle, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetOrdinal, L0::zeCommandListGetOrdinal, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnImmediateGetIndex, L0::zeCommandListImmediateGetIndex, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnIsImmediate, L0::zeCommandListIsImmediate, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.CommandList = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnAppendBarrier, zeCommandListAppendBarrierTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendMemoryRangesBarrier, zeCommandListAppendMemoryRangesBarrierTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnCreate, zeCommandListCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnCreateImmediate, zeCommandListCreateImmediateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeCommandListDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnClose, zeCommandListCloseTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnReset, zeCommandListResetTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendMemoryCopy, zeCommandListAppendMemoryCopyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendMemoryCopyRegion, zeCommandListAppendMemoryCopyRegionTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendMemoryFill, zeCommandListAppendMemoryFillTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendImageCopy, zeCommandListAppendImageCopyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendImageCopyRegion, zeCommandListAppendImageCopyRegionTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendImageCopyToMemory, zeCommandListAppendImageCopyToMemoryTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendImageCopyFromMemory, zeCommandListAppendImageCopyFromMemoryTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendMemoryPrefetch, zeCommandListAppendMemoryPrefetchTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendMemAdvise, zeCommandListAppendMemAdviseTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendSignalEvent, zeCommandListAppendSignalEventTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendWaitOnEvents, zeCommandListAppendWaitOnEventsTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendEventReset, zeCommandListAppendEventResetTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendLaunchKernel, zeCommandListAppendLaunchKernelTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendLaunchCooperativeKernel, zeCommandListAppendLaunchCooperativeKernelTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendLaunchKernelIndirect, zeCommandListAppendLaunchKernelIndirectTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendLaunchMultipleKernelsIndirect, zeCommandListAppendLaunchMultipleKernelsIndirectTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendWriteGlobalTimestamp, zeCommandListAppendWriteGlobalTimestampTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendMemoryCopyFromContext, zeCommandListAppendMemoryCopyFromContextTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnAppendQueryKernelTimestamps, zeCommandListAppendQueryKernelTimestampsTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandListExpProcAddrTable(
    ze_api_version_t version,
    ze_command_list_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnImmediateAppendCommandListsExp, L0::zeCommandListImmediateAppendCommandListsExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetNextCommandIdExp, L0::zeCommandListGetNextCommandIdExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandsExp, L0::zeCommandListUpdateMutableCommandsExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandSignalEventExp, L0::zeCommandListUpdateMutableCommandSignalEventExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandWaitEventsExp, L0::zeCommandListUpdateMutableCommandWaitEventsExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetNextCommandIdWithKernelsExp, L0::zeCommandListGetNextCommandIdWithKernelsExp, version, ZE_API_VERSION_1_10);
    fillDdiEntry(pDdiTable->pfnUpdateMutableCommandKernelsExp, L0::zeCommandListUpdateMutableCommandKernelsExp, version, ZE_API_VERSION_1_10);
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFenceProcAddrTable(
    ze_api_version_t version,
    ze_fence_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeFenceCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeFenceDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSynchronize, L0::zeFenceHostSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryStatus, L0::zeFenceQueryStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::zeFenceReset, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Fence = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeFenceCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeFenceDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnHostSynchronize, zeFenceHostSynchronizeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnQueryStatus, zeFenceQueryStatusTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnReset, zeFenceResetTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventPoolProcAddrTable(
    ze_api_version_t version,
    ze_event_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeEventPoolCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeEventPoolDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIpcHandle, L0::zeEventPoolGetIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnOpenIpcHandle, L0::zeEventPoolOpenIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCloseIpcHandle, L0::zeEventPoolCloseIpcHandle, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetContextHandle, L0::zeEventPoolGetContextHandle, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetFlags, L0::zeEventPoolGetFlags, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.EventPool = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeEventPoolCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeEventPoolDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetIpcHandle, zeEventPoolGetIpcHandleTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnOpenIpcHandle, zeEventPoolOpenIpcHandleTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnCloseIpcHandle, zeEventPoolCloseIpcHandleTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventProcAddrTable(
    ze_api_version_t version,
    ze_event_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeEventCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeEventDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSignal, L0::zeEventHostSignal, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostSynchronize, L0::zeEventHostSynchronize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryStatus, L0::zeEventQueryStatus, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnHostReset, L0::zeEventHostReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryKernelTimestamp, L0::zeEventQueryKernelTimestamp, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnQueryKernelTimestampsExt, L0::zeEventQueryKernelTimestampsExt, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnGetEventPool, L0::zeEventGetEventPool, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetSignalScope, L0::zeEventGetSignalScope, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetWaitScope, L0::zeEventGetWaitScope, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.Event = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeEventCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeEventDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnHostSignal, zeEventHostSignalTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnHostSynchronize, zeEventHostSynchronizeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnQueryStatus, zeEventQueryStatusTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnHostReset, zeEventHostResetTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnQueryKernelTimestamp, zeEventQueryKernelTimestampTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventExpProcAddrTable(
    ze_api_version_t version,
    ze_event_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnQueryTimestampsExp, L0::zeEventQueryTimestampsExp, version, ZE_API_VERSION_1_2);

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageProcAddrTable(
    ze_api_version_t version,
    ze_image_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zeImageGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeImageCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeImageDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetAllocPropertiesExt, L0::zeImageGetAllocPropertiesExt, version, ZE_API_VERSION_1_3);
    fillDdiEntry(pDdiTable->pfnViewCreateExt, L0::zeImageViewCreateExt, version, ZE_API_VERSION_1_5);
    driverDdiTable.coreDdiTable.Image = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnGetProperties, zeImageGetPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnCreate, zeImageCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeImageDestroyTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleProcAddrTable(
    ze_api_version_t version,
    ze_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeModuleCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeModuleDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDynamicLink, L0::zeModuleDynamicLink, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetNativeBinary, L0::zeModuleGetNativeBinary, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetGlobalPointer, L0::zeModuleGetGlobalPointer, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetKernelNames, L0::zeModuleGetKernelNames, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetFunctionPointer, L0::zeModuleGetFunctionPointer, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zeModuleGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnInspectLinkageExt, L0::zeModuleInspectLinkageExt, version, ZE_API_VERSION_1_3);
    driverDdiTable.coreDdiTable.Module = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeModuleCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeModuleDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetNativeBinary, zeModuleGetNativeBinaryTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDynamicLink, zeModuleDynamicLinkTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetGlobalPointer, zeModuleGetGlobalPointerTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetFunctionPointer, zeModuleGetFunctionPointerTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetKernelNames, zeModuleGetKernelNamesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetProperties, zeModuleGetPropertiesTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleBuildLogProcAddrTable(
    ze_api_version_t version,
    ze_module_build_log_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeModuleBuildLogDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetString, L0::zeModuleBuildLogGetString, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.ModuleBuildLog = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnDestroy, zeModuleBuildLogDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetString, zeModuleBuildLogGetStringTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetKernelProcAddrTable(
    ze_api_version_t version,
    ze_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeKernelCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeKernelDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetGroupSize, L0::zeKernelSetGroupSize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSuggestGroupSize, L0::zeKernelSuggestGroupSize, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSuggestMaxCooperativeGroupCount, L0::zeKernelSuggestMaxCooperativeGroupCount, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetArgumentValue, L0::zeKernelSetArgumentValue, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetIndirectAccess, L0::zeKernelSetIndirectAccess, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetIndirectAccess, L0::zeKernelGetIndirectAccess, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetSourceAttributes, L0::zeKernelGetSourceAttributes, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zeKernelGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetCacheConfig, L0::zeKernelSetCacheConfig, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetName, L0::zeKernelGetName, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Kernel = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeKernelCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeKernelDestroyTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSetGroupSize, zeKernelSetGroupSizeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSuggestGroupSize, zeKernelSuggestGroupSizeTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSuggestMaxCooperativeGroupCount, zeKernelSuggestMaxCooperativeGroupCountTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSetArgumentValue, zeKernelSetArgumentValueTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSetIndirectAccess, zeKernelSetIndirectAccessTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetIndirectAccess, zeKernelGetIndirectAccessTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetSourceAttributes, zeKernelGetSourceAttributesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetProperties, zeKernelGetPropertiesTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnSetCacheConfig, zeKernelSetCacheConfigTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnGetName, zeKernelGetNameTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetSamplerProcAddrTable(
    ze_api_version_t version,
    ze_sampler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    driverDdiTable.enableTracing = NEO::IoFunctions::getEnvToBool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zeSamplerCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zeSamplerDestroy, version, ZE_API_VERSION_1_0);
    driverDdiTable.coreDdiTable.Sampler = *pDdiTable;
    if (driverDdiTable.enableTracing) {
        fillDdiEntry(pDdiTable->pfnCreate, zeSamplerCreateTracing, version, ZE_API_VERSION_1_0);
        fillDdiEntry(pDdiTable->pfnDestroy, zeSamplerDestroyTracing, version, ZE_API_VERSION_1_0);
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetKernelExpProcAddrTable(
    ze_api_version_t version,
    ze_kernel_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnSetGlobalOffsetExp, L0::zeKernelSetGlobalOffsetExp, version, ZE_API_VERSION_1_1);
    fillDdiEntry(pDdiTable->pfnSchedulingHintExp, L0::zeKernelSchedulingHintExp, version, ZE_API_VERSION_1_2);
    driverDdiTable.coreDdiTable.KernelExp = *pDdiTable;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetMemExpProcAddrTable(
    ze_api_version_t version,
    ze_mem_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetIpcHandleFromFileDescriptorExp, L0::zeMemGetIpcHandleFromFileDescriptorExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnGetFileDescriptorFromIpcHandleExp, L0::zeMemGetFileDescriptorFromIpcHandleExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnSetAtomicAccessAttributeExp, L0::zeMemSetAtomicAccessAttributeExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetAtomicAccessAttributeExp, L0::zeMemGetAtomicAccessAttributeExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.MemExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageExpProcAddrTable(
    ze_api_version_t version,
    ze_image_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetMemoryPropertiesExp, L0::zeImageGetMemoryPropertiesExp, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnViewCreateExp, L0::zeImageViewCreateExp, version, ZE_API_VERSION_1_2);
    fillDdiEntry(pDdiTable->pfnGetDeviceOffsetExp, L0::zeImageGetDeviceOffsetExp, version, ZE_API_VERSION_1_9);
    driverDdiTable.coreDdiTable.ImageExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFabricVertexExpProcAddrTable(
    ze_api_version_t version,
    ze_fabric_vertex_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetExp, L0::zeFabricVertexGetExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetSubVerticesExp, L0::zeFabricVertexGetSubVerticesExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::zeFabricVertexGetPropertiesExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetDeviceExp, L0::zeFabricVertexGetDeviceExp, version, ZE_API_VERSION_1_4);
    driverDdiTable.coreDdiTable.FabricVertexExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFabricEdgeExpProcAddrTable(
    ze_api_version_t version,
    ze_fabric_edge_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetExp, L0::zeFabricEdgeGetExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetVerticesExp, L0::zeFabricEdgeGetVerticesExp, version, ZE_API_VERSION_1_4);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::zeFabricEdgeGetPropertiesExp, version, ZE_API_VERSION_1_4);
    driverDdiTable.coreDdiTable.FabricEdgeExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDriverExpProcAddrTable(
    ze_api_version_t version,
    ze_driver_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnRTASFormatCompatibilityCheckExp, L0::zeDriverRTASFormatCompatibilityCheckExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.DriverExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASParallelOperationExpProcAddrTable(
    ze_api_version_t version,
    ze_rtas_parallel_operation_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateExp, L0::zeRTASParallelOperationCreateExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::zeRTASParallelOperationGetPropertiesExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnJoinExp, L0::zeRTASParallelOperationJoinExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::zeRTASParallelOperationDestroyExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.RTASParallelOperationExp = *pDdiTable;
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetRTASBuilderExpProcAddrTable(
    ze_api_version_t version,
    ze_rtas_builder_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateExp, L0::zeRTASBuilderCreateExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnGetBuildPropertiesExp, L0::zeRTASBuilderGetBuildPropertiesExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnBuildExp, L0::zeRTASBuilderBuildExp, version, ZE_API_VERSION_1_7);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::zeRTASBuilderDestroyExp, version, ZE_API_VERSION_1_7);
    driverDdiTable.coreDdiTable.RTASBuilderExp = *pDdiTable;
    return result;
}
