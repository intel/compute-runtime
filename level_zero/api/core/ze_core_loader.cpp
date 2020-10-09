/*
 * Copyright (C) 2019-2020 Intel Corporation
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
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = zeDriverGet;
    pDdiTable->pfnGetApiVersion = zeDriverGetApiVersion;
    pDdiTable->pfnGetProperties = zeDriverGetProperties;
    pDdiTable->pfnGetIpcProperties = zeDriverGetIpcProperties;
    pDdiTable->pfnGetExtensionProperties = zeDriverGetExtensionProperties;
    driver_ddiTable.core_ddiTable.Driver = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGet = zeDriverGet_Tracing;
        if (nullptr == pDdiTable->pfnGet) {
            pDdiTable->pfnGet = driver_ddiTable.core_ddiTable.Driver.pfnGet;
        }
        pDdiTable->pfnGetApiVersion = zeDriverGetApiVersion_Tracing;
        if (nullptr == pDdiTable->pfnGetApiVersion) {
            pDdiTable->pfnGetApiVersion = driver_ddiTable.core_ddiTable.Driver.pfnGetApiVersion;
        }
        pDdiTable->pfnGetProperties = zeDriverGetProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Driver.pfnGetProperties;
        }
        pDdiTable->pfnGetIpcProperties = zeDriverGetIpcProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetIpcProperties) {
            pDdiTable->pfnGetIpcProperties = driver_ddiTable.core_ddiTable.Driver.pfnGetIpcProperties;
        }
        pDdiTable->pfnGetExtensionProperties = zeDriverGetExtensionProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetExtensionProperties) {
            pDdiTable->pfnGetExtensionProperties = driver_ddiTable.core_ddiTable.Driver.pfnGetExtensionProperties;
        }
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetMemProcAddrTable(
    ze_api_version_t version,
    ze_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAllocShared = zeMemAllocShared;
    pDdiTable->pfnAllocDevice = zeMemAllocDevice;
    pDdiTable->pfnAllocHost = zeMemAllocHost;
    pDdiTable->pfnFree = zeMemFree;
    pDdiTable->pfnGetAllocProperties = zeMemGetAllocProperties;
    pDdiTable->pfnGetAddressRange = zeMemGetAddressRange;
    pDdiTable->pfnGetIpcHandle = zeMemGetIpcHandle;
    pDdiTable->pfnOpenIpcHandle = zeMemOpenIpcHandle;
    pDdiTable->pfnCloseIpcHandle = zeMemCloseIpcHandle;
    driver_ddiTable.core_ddiTable.Mem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnAllocShared = zeMemAllocShared_Tracing;
        if (nullptr == pDdiTable->pfnAllocShared) {
            pDdiTable->pfnAllocShared = driver_ddiTable.core_ddiTable.Mem.pfnAllocShared;
        }
        pDdiTable->pfnAllocDevice = zeMemAllocDevice_Tracing;
        if (nullptr == pDdiTable->pfnAllocDevice) {
            pDdiTable->pfnAllocDevice = driver_ddiTable.core_ddiTable.Mem.pfnAllocDevice;
        }
        pDdiTable->pfnAllocHost = zeMemAllocHost_Tracing;
        if (nullptr == pDdiTable->pfnAllocHost) {
            pDdiTable->pfnAllocHost = driver_ddiTable.core_ddiTable.Mem.pfnAllocHost;
        }
        pDdiTable->pfnFree = zeMemFree_Tracing;
        if (nullptr == pDdiTable->pfnAllocHost) {
            pDdiTable->pfnAllocHost = driver_ddiTable.core_ddiTable.Mem.pfnAllocHost;
        }
        pDdiTable->pfnGetAllocProperties = zeMemGetAllocProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetAllocProperties) {
            pDdiTable->pfnGetAllocProperties = driver_ddiTable.core_ddiTable.Mem.pfnGetAllocProperties;
        }
        pDdiTable->pfnGetAddressRange = zeMemGetAddressRange_Tracing;
        if (nullptr == pDdiTable->pfnGetAddressRange) {
            pDdiTable->pfnGetAddressRange = driver_ddiTable.core_ddiTable.Mem.pfnGetAddressRange;
        }
        pDdiTable->pfnGetIpcHandle = zeMemGetIpcHandle_Tracing;
        if (nullptr == pDdiTable->pfnGetIpcHandle) {
            pDdiTable->pfnGetIpcHandle = driver_ddiTable.core_ddiTable.Mem.pfnGetIpcHandle;
        }
        pDdiTable->pfnOpenIpcHandle = zeMemOpenIpcHandle_Tracing;
        if (nullptr == pDdiTable->pfnOpenIpcHandle) {
            pDdiTable->pfnOpenIpcHandle = driver_ddiTable.core_ddiTable.Mem.pfnOpenIpcHandle;
        }
        pDdiTable->pfnCloseIpcHandle = zeMemCloseIpcHandle_Tracing;
        if (nullptr == pDdiTable->pfnCloseIpcHandle) {
            pDdiTable->pfnCloseIpcHandle = driver_ddiTable.core_ddiTable.Mem.pfnCloseIpcHandle;
        }
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetContextProcAddrTable(
    ze_api_version_t version,
    ze_context_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;

    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeContextCreate;
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
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Context.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeContextDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Context.pfnDestroy;
        }
        pDdiTable->pfnGetStatus = zeContextGetStatus_Tracing;
        if (nullptr == pDdiTable->pfnGetStatus) {
            pDdiTable->pfnGetStatus = driver_ddiTable.core_ddiTable.Context.pfnGetStatus;
        }
        pDdiTable->pfnSystemBarrier = zeContextSystemBarrier_Tracing;
        if (nullptr == pDdiTable->pfnSystemBarrier) {
            pDdiTable->pfnSystemBarrier = driver_ddiTable.core_ddiTable.Context.pfnSystemBarrier;
        }
        pDdiTable->pfnMakeMemoryResident = zeContextMakeMemoryResident_Tracing;
        if (nullptr == pDdiTable->pfnMakeMemoryResident) {
            pDdiTable->pfnMakeMemoryResident = driver_ddiTable.core_ddiTable.Context.pfnMakeMemoryResident;
        }
        pDdiTable->pfnEvictMemory = zeContextEvictMemory_Tracing;
        if (nullptr == pDdiTable->pfnEvictMemory) {
            pDdiTable->pfnEvictMemory = driver_ddiTable.core_ddiTable.Context.pfnEvictMemory;
        }
        pDdiTable->pfnMakeImageResident = zeContextMakeImageResident_Tracing;
        if (nullptr == pDdiTable->pfnMakeImageResident) {
            pDdiTable->pfnMakeImageResident = driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident;
        }
        pDdiTable->pfnEvictImage = zeContextEvictImage_Tracing;
        if (nullptr == pDdiTable->pfnEvictImage) {
            pDdiTable->pfnEvictImage = driver_ddiTable.core_ddiTable.Context.pfnEvictImage;
        }
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetPhysicalMemProcAddrTable(
    ze_api_version_t version,
    ze_physical_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;

    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zePhysicalMemCreate;
    pDdiTable->pfnDestroy = zePhysicalMemDestroy;

    driver_ddiTable.core_ddiTable.PhysicalMem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zePhysicalMemCreate_Tracing;
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.PhysicalMem.pfnCreate;
        }
        pDdiTable->pfnDestroy = zePhysicalMemDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.PhysicalMem.pfnDestroy;
        }
    }
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zeGetVirtualMemProcAddrTable(
    ze_api_version_t version,
    ze_virtual_mem_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
        if (nullptr == pDdiTable->pfnReserve) {
            pDdiTable->pfnReserve = driver_ddiTable.core_ddiTable.VirtualMem.pfnReserve;
        }
        pDdiTable->pfnFree = zeVirtualMemFree_Tracing;
        if (nullptr == pDdiTable->pfnFree) {
            pDdiTable->pfnFree = driver_ddiTable.core_ddiTable.VirtualMem.pfnFree;
        }
        pDdiTable->pfnQueryPageSize = zeVirtualMemQueryPageSize_Tracing;
        if (nullptr == pDdiTable->pfnQueryPageSize) {
            pDdiTable->pfnQueryPageSize = driver_ddiTable.core_ddiTable.VirtualMem.pfnQueryPageSize;
        }
        pDdiTable->pfnMap = zeVirtualMemMap_Tracing;
        if (nullptr == pDdiTable->pfnMap) {
            pDdiTable->pfnMap = driver_ddiTable.core_ddiTable.VirtualMem.pfnMap;
        }
        pDdiTable->pfnUnmap = zeVirtualMemUnmap_Tracing;
        if (nullptr == pDdiTable->pfnUnmap) {
            pDdiTable->pfnUnmap = driver_ddiTable.core_ddiTable.VirtualMem.pfnUnmap;
        }
        pDdiTable->pfnSetAccessAttribute = zeVirtualMemSetAccessAttribute_Tracing;
        if (nullptr == pDdiTable->pfnSetAccessAttribute) {
            pDdiTable->pfnSetAccessAttribute = driver_ddiTable.core_ddiTable.VirtualMem.pfnSetAccessAttribute;
        }
        pDdiTable->pfnGetAccessAttribute = zeVirtualMemGetAccessAttribute_Tracing;
        if (nullptr == pDdiTable->pfnGetAccessAttribute) {
            pDdiTable->pfnGetAccessAttribute = driver_ddiTable.core_ddiTable.VirtualMem.pfnGetAccessAttribute;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetGlobalProcAddrTable(
    ze_api_version_t version,
    ze_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnInit = zeInit;
    driver_ddiTable.core_ddiTable.Global = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnInit = zeInit_Tracing;
        if (nullptr == pDdiTable->pfnInit) {
            pDdiTable->pfnInit = driver_ddiTable.core_ddiTable.Global.pfnInit;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetDeviceProcAddrTable(
    ze_api_version_t version,
    ze_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
    driver_ddiTable.core_ddiTable.Device = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGet = zeDeviceGet_Tracing;
        if (nullptr == pDdiTable->pfnGet) {
            pDdiTable->pfnGet = driver_ddiTable.core_ddiTable.Device.pfnGet;
        }
        pDdiTable->pfnGetCommandQueueGroupProperties = zeDeviceGetCommandQueueGroupProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetCommandQueueGroupProperties) {
            pDdiTable->pfnGetCommandQueueGroupProperties = driver_ddiTable.core_ddiTable.Device.pfnGetCommandQueueGroupProperties;
        }
        pDdiTable->pfnGetSubDevices = zeDeviceGetSubDevices_Tracing;
        if (nullptr == pDdiTable->pfnGetSubDevices) {
            pDdiTable->pfnGetSubDevices = driver_ddiTable.core_ddiTable.Device.pfnGetSubDevices;
        }
        pDdiTable->pfnGetProperties = zeDeviceGetProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Device.pfnGetProperties;
        }
        pDdiTable->pfnGetComputeProperties = zeDeviceGetComputeProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetComputeProperties) {
            pDdiTable->pfnGetComputeProperties = driver_ddiTable.core_ddiTable.Device.pfnGetComputeProperties;
        }
        pDdiTable->pfnGetModuleProperties = zeDeviceGetModuleProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetModuleProperties) {
            pDdiTable->pfnGetModuleProperties = driver_ddiTable.core_ddiTable.Device.pfnGetModuleProperties;
        }
        pDdiTable->pfnGetMemoryProperties = zeDeviceGetMemoryProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetMemoryProperties) {
            pDdiTable->pfnGetMemoryProperties = driver_ddiTable.core_ddiTable.Device.pfnGetMemoryProperties;
        }
        pDdiTable->pfnGetMemoryAccessProperties = zeDeviceGetMemoryAccessProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetMemoryAccessProperties) {
            pDdiTable->pfnGetMemoryAccessProperties = driver_ddiTable.core_ddiTable.Device.pfnGetMemoryAccessProperties;
        }
        pDdiTable->pfnGetCacheProperties = zeDeviceGetCacheProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetCacheProperties) {
            pDdiTable->pfnGetCacheProperties = driver_ddiTable.core_ddiTable.Device.pfnGetCacheProperties;
        }
        pDdiTable->pfnGetImageProperties = zeDeviceGetImageProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetImageProperties) {
            pDdiTable->pfnGetImageProperties = driver_ddiTable.core_ddiTable.Device.pfnGetImageProperties;
        }
        pDdiTable->pfnGetP2PProperties = zeDeviceGetP2PProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetP2PProperties) {
            pDdiTable->pfnGetP2PProperties = driver_ddiTable.core_ddiTable.Device.pfnGetP2PProperties;
        }
        pDdiTable->pfnCanAccessPeer = zeDeviceCanAccessPeer_Tracing;
        if (nullptr == pDdiTable->pfnCanAccessPeer) {
            pDdiTable->pfnCanAccessPeer = driver_ddiTable.core_ddiTable.Device.pfnCanAccessPeer;
        }
        pDdiTable->pfnGetStatus = zeDeviceGetStatus_Tracing;
        if (nullptr == pDdiTable->pfnGetStatus) {
            pDdiTable->pfnGetStatus = driver_ddiTable.core_ddiTable.Device.pfnGetStatus;
        }
        pDdiTable->pfnGetExternalMemoryProperties = zeDeviceGetExternalMemoryProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetExternalMemoryProperties) {
            pDdiTable->pfnGetExternalMemoryProperties = driver_ddiTable.core_ddiTable.Device.pfnGetExternalMemoryProperties;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandQueueProcAddrTable(
    ze_api_version_t version,
    ze_command_queue_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeCommandQueueCreate;
    pDdiTable->pfnDestroy = zeCommandQueueDestroy;
    pDdiTable->pfnExecuteCommandLists = zeCommandQueueExecuteCommandLists;
    pDdiTable->pfnSynchronize = zeCommandQueueSynchronize;
    driver_ddiTable.core_ddiTable.CommandQueue = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeCommandQueueCreate_Tracing;
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.CommandQueue.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeCommandQueueDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.CommandQueue.pfnDestroy;
        }
        pDdiTable->pfnExecuteCommandLists = zeCommandQueueExecuteCommandLists_Tracing;
        if (nullptr == pDdiTable->pfnExecuteCommandLists) {
            pDdiTable->pfnExecuteCommandLists = driver_ddiTable.core_ddiTable.CommandQueue.pfnExecuteCommandLists;
        }
        pDdiTable->pfnSynchronize = zeCommandQueueSynchronize_Tracing;
        if (nullptr == pDdiTable->pfnSynchronize) {
            pDdiTable->pfnSynchronize = driver_ddiTable.core_ddiTable.CommandQueue.pfnSynchronize;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetCommandListProcAddrTable(
    ze_api_version_t version,
    ze_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
        if (nullptr == pDdiTable->pfnAppendBarrier) {
            pDdiTable->pfnAppendBarrier = driver_ddiTable.core_ddiTable.CommandList.pfnAppendBarrier;
        }
        pDdiTable->pfnAppendMemoryRangesBarrier = zeCommandListAppendMemoryRangesBarrier_Tracing;
        if (nullptr == pDdiTable->pfnAppendMemoryRangesBarrier) {
            pDdiTable->pfnAppendMemoryRangesBarrier = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryRangesBarrier;
        }
        pDdiTable->pfnCreate = zeCommandListCreate_Tracing;
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.CommandList.pfnCreate;
        }
        pDdiTable->pfnCreateImmediate = zeCommandListCreateImmediate_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.CommandList.pfnDestroy;
        }
        pDdiTable->pfnDestroy = zeCommandListDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.CommandList.pfnDestroy;
        }
        pDdiTable->pfnClose = zeCommandListClose_Tracing;
        if (nullptr == pDdiTable->pfnClose) {
            pDdiTable->pfnClose = driver_ddiTable.core_ddiTable.CommandList.pfnClose;
        }
        pDdiTable->pfnReset = zeCommandListReset_Tracing;
        if (nullptr == pDdiTable->pfnReset) {
            pDdiTable->pfnReset = driver_ddiTable.core_ddiTable.CommandList.pfnReset;
        }
        pDdiTable->pfnAppendMemoryCopy = zeCommandListAppendMemoryCopy_Tracing;
        if (nullptr == pDdiTable->pfnAppendMemoryCopy) {
            pDdiTable->pfnAppendMemoryCopy = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopy;
        }
        pDdiTable->pfnAppendMemoryCopyRegion = zeCommandListAppendMemoryCopyRegion_Tracing;
        if (nullptr == pDdiTable->pfnAppendMemoryCopyRegion) {
            pDdiTable->pfnAppendMemoryCopyRegion = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyRegion;
        }
        pDdiTable->pfnAppendMemoryFill = zeCommandListAppendMemoryFill_Tracing;
        if (nullptr == pDdiTable->pfnAppendMemoryFill) {
            pDdiTable->pfnAppendMemoryFill = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryFill;
        }
        pDdiTable->pfnAppendImageCopy = zeCommandListAppendImageCopy_Tracing;
        if (nullptr == pDdiTable->pfnAppendImageCopy) {
            pDdiTable->pfnAppendImageCopy = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopy;
        }
        pDdiTable->pfnAppendImageCopyRegion = zeCommandListAppendImageCopyRegion_Tracing;
        if (nullptr == pDdiTable->pfnAppendImageCopyRegion) {
            pDdiTable->pfnAppendImageCopyRegion = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyRegion;
        }
        pDdiTable->pfnAppendImageCopyToMemory = zeCommandListAppendImageCopyToMemory_Tracing;
        if (nullptr == pDdiTable->pfnAppendImageCopyToMemory) {
            pDdiTable->pfnAppendImageCopyToMemory = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyToMemory;
        }
        pDdiTable->pfnAppendImageCopyFromMemory = zeCommandListAppendImageCopyFromMemory_Tracing;
        if (nullptr == pDdiTable->pfnAppendImageCopyFromMemory) {
            pDdiTable->pfnAppendImageCopyFromMemory = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyFromMemory;
        }
        pDdiTable->pfnAppendMemoryPrefetch = zeCommandListAppendMemoryPrefetch_Tracing;
        if (nullptr == pDdiTable->pfnAppendMemoryPrefetch) {
            pDdiTable->pfnAppendMemoryPrefetch = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryPrefetch;
        }
        pDdiTable->pfnAppendMemAdvise = zeCommandListAppendMemAdvise_Tracing;
        if (nullptr == pDdiTable->pfnAppendMemAdvise) {
            pDdiTable->pfnAppendMemAdvise = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemAdvise;
        }
        pDdiTable->pfnAppendSignalEvent = zeCommandListAppendSignalEvent_Tracing;
        if (nullptr == pDdiTable->pfnAppendSignalEvent) {
            pDdiTable->pfnAppendSignalEvent = driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent;
        }
        pDdiTable->pfnAppendWaitOnEvents = zeCommandListAppendWaitOnEvents_Tracing;
        if (nullptr == pDdiTable->pfnAppendWaitOnEvents) {
            pDdiTable->pfnAppendWaitOnEvents = driver_ddiTable.core_ddiTable.CommandList.pfnAppendWaitOnEvents;
        }
        pDdiTable->pfnAppendEventReset = zeCommandListAppendEventReset_Tracing;
        if (nullptr == pDdiTable->pfnAppendEventReset) {
            pDdiTable->pfnAppendEventReset = driver_ddiTable.core_ddiTable.CommandList.pfnAppendEventReset;
        }
        pDdiTable->pfnAppendLaunchKernel = zeCommandListAppendLaunchKernel_Tracing;
        if (nullptr == pDdiTable->pfnAppendLaunchKernel) {
            pDdiTable->pfnAppendLaunchKernel = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernel;
        }
        pDdiTable->pfnAppendLaunchCooperativeKernel = zeCommandListAppendLaunchCooperativeKernel_Tracing;
        if (nullptr == pDdiTable->pfnAppendLaunchCooperativeKernel) {
            pDdiTable->pfnAppendLaunchCooperativeKernel = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchCooperativeKernel;
        }
        pDdiTable->pfnAppendLaunchKernelIndirect = zeCommandListAppendLaunchKernelIndirect_Tracing;
        if (nullptr == pDdiTable->pfnAppendLaunchKernelIndirect) {
            pDdiTable->pfnAppendLaunchKernelIndirect = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernelIndirect;
        }
        pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = zeCommandListAppendLaunchMultipleKernelsIndirect_Tracing;
        if (nullptr == pDdiTable->pfnAppendLaunchMultipleKernelsIndirect) {
            pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchMultipleKernelsIndirect;
        }
        pDdiTable->pfnAppendWriteGlobalTimestamp = zeCommandListAppendWriteGlobalTimestamp_Tracing;
        if (nullptr == pDdiTable->pfnAppendWriteGlobalTimestamp) {
            pDdiTable->pfnAppendWriteGlobalTimestamp = driver_ddiTable.core_ddiTable.CommandList.pfnAppendWriteGlobalTimestamp;
        }
        pDdiTable->pfnAppendMemoryCopyFromContext = zeCommandListAppendMemoryCopyFromContext_Tracing;
        if (nullptr == pDdiTable->pfnAppendMemoryCopyFromContext) {
            pDdiTable->pfnAppendMemoryCopyFromContext = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyFromContext;
        }
        pDdiTable->pfnAppendQueryKernelTimestamps = zeCommandListAppendQueryKernelTimestamps_Tracing;
        if (nullptr == pDdiTable->pfnAppendQueryKernelTimestamps) {
            pDdiTable->pfnAppendQueryKernelTimestamps = driver_ddiTable.core_ddiTable.CommandList.pfnAppendQueryKernelTimestamps;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetFenceProcAddrTable(
    ze_api_version_t version,
    ze_fence_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Fence.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeFenceDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Fence.pfnDestroy;
        }
        pDdiTable->pfnHostSynchronize = zeFenceHostSynchronize_Tracing;
        if (nullptr == pDdiTable->pfnHostSynchronize) {
            pDdiTable->pfnHostSynchronize = driver_ddiTable.core_ddiTable.Fence.pfnHostSynchronize;
        }
        pDdiTable->pfnQueryStatus = zeFenceQueryStatus_Tracing;
        if (nullptr == pDdiTable->pfnQueryStatus) {
            pDdiTable->pfnQueryStatus = driver_ddiTable.core_ddiTable.Fence.pfnQueryStatus;
        }
        pDdiTable->pfnReset = zeFenceReset_Tracing;
        if (nullptr == pDdiTable->pfnReset) {
            pDdiTable->pfnReset = driver_ddiTable.core_ddiTable.Fence.pfnReset;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventPoolProcAddrTable(
    ze_api_version_t version,
    ze_event_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.EventPool.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeEventPoolDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.EventPool.pfnDestroy;
        }
        pDdiTable->pfnGetIpcHandle = zeEventPoolGetIpcHandle_Tracing;
        if (nullptr == pDdiTable->pfnGetIpcHandle) {
            pDdiTable->pfnGetIpcHandle = driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle;
        }
        pDdiTable->pfnOpenIpcHandle = zeEventPoolOpenIpcHandle_Tracing;
        if (nullptr == pDdiTable->pfnOpenIpcHandle) {
            pDdiTable->pfnOpenIpcHandle = driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle;
        }
        pDdiTable->pfnCloseIpcHandle = zeEventPoolCloseIpcHandle_Tracing;
        if (nullptr == pDdiTable->pfnCloseIpcHandle) {
            pDdiTable->pfnCloseIpcHandle = driver_ddiTable.core_ddiTable.EventPool.pfnCloseIpcHandle;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetEventProcAddrTable(
    ze_api_version_t version,
    ze_event_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Event.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeEventDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Event.pfnDestroy;
        }
        pDdiTable->pfnHostSignal = zeEventHostSignal_Tracing;
        if (nullptr == pDdiTable->pfnHostSignal) {
            pDdiTable->pfnHostSignal = driver_ddiTable.core_ddiTable.Event.pfnHostSignal;
        }
        pDdiTable->pfnHostSynchronize = zeEventHostSynchronize_Tracing;
        if (nullptr == pDdiTable->pfnHostSynchronize) {
            pDdiTable->pfnHostSynchronize = driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize;
        }
        pDdiTable->pfnQueryStatus = zeEventQueryStatus_Tracing;
        if (nullptr == pDdiTable->pfnQueryStatus) {
            pDdiTable->pfnQueryStatus = driver_ddiTable.core_ddiTable.Event.pfnQueryStatus;
        }
        pDdiTable->pfnHostReset = zeEventHostReset_Tracing;
        if (nullptr == pDdiTable->pfnHostReset) {
            pDdiTable->pfnHostReset = driver_ddiTable.core_ddiTable.Event.pfnHostReset;
        }
        pDdiTable->pfnQueryKernelTimestamp = zeEventQueryKernelTimestamp_Tracing;
        if (nullptr == pDdiTable->pfnQueryKernelTimestamp) {
            pDdiTable->pfnQueryKernelTimestamp = driver_ddiTable.core_ddiTable.Event.pfnQueryKernelTimestamp;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetImageProcAddrTable(
    ze_api_version_t version,
    ze_image_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = zeImageGetProperties;
    pDdiTable->pfnCreate = zeImageCreate;
    pDdiTable->pfnDestroy = zeImageDestroy;
    driver_ddiTable.core_ddiTable.Image = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGetProperties = zeImageGetProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Image.pfnGetProperties;
        }
        pDdiTable->pfnCreate = zeImageCreate_Tracing;
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Image.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeImageDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Image.pfnDestroy;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleProcAddrTable(
    ze_api_version_t version,
    ze_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Module.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeModuleDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Module.pfnDestroy;
        }
        pDdiTable->pfnGetNativeBinary = zeModuleGetNativeBinary_Tracing;
        if (nullptr == pDdiTable->pfnGetNativeBinary) {
            pDdiTable->pfnGetNativeBinary = driver_ddiTable.core_ddiTable.Module.pfnGetNativeBinary;
        }
        pDdiTable->pfnDynamicLink = zeModuleDynamicLink_Tracing;
        if (nullptr == pDdiTable->pfnDynamicLink) {
            pDdiTable->pfnDynamicLink = driver_ddiTable.core_ddiTable.Module.pfnDynamicLink;
        }
        pDdiTable->pfnGetGlobalPointer = zeModuleGetGlobalPointer_Tracing;
        if (nullptr == pDdiTable->pfnGetGlobalPointer) {
            pDdiTable->pfnGetGlobalPointer = driver_ddiTable.core_ddiTable.Module.pfnGetGlobalPointer;
        }
        pDdiTable->pfnGetFunctionPointer = zeModuleGetFunctionPointer_Tracing;
        if (nullptr == pDdiTable->pfnGetFunctionPointer) {
            pDdiTable->pfnGetFunctionPointer = driver_ddiTable.core_ddiTable.Module.pfnGetFunctionPointer;
        }
        pDdiTable->pfnGetKernelNames = zeModuleGetKernelNames_Tracing;
        if (nullptr == pDdiTable->pfnGetKernelNames) {
            pDdiTable->pfnGetKernelNames = driver_ddiTable.core_ddiTable.Module.pfnGetKernelNames;
        }
        pDdiTable->pfnGetProperties = zeModuleGetProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Module.pfnGetProperties;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetModuleBuildLogProcAddrTable(
    ze_api_version_t version,
    ze_module_build_log_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnDestroy = zeModuleBuildLogDestroy;
    pDdiTable->pfnGetString = zeModuleBuildLogGetString;
    driver_ddiTable.core_ddiTable.ModuleBuildLog = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnDestroy = zeModuleBuildLogDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnDestroy;
        }
        pDdiTable->pfnGetString = zeModuleBuildLogGetString_Tracing;
        if (nullptr == pDdiTable->pfnGetString) {
            pDdiTable->pfnGetString = driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnGetString;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetKernelProcAddrTable(
    ze_api_version_t version,
    ze_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
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
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Kernel.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeKernelDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Kernel.pfnDestroy;
        }
        pDdiTable->pfnSetGroupSize = zeKernelSetGroupSize_Tracing;
        if (nullptr == pDdiTable->pfnSetGroupSize) {
            pDdiTable->pfnSetGroupSize = driver_ddiTable.core_ddiTable.Kernel.pfnSetGroupSize;
        }
        pDdiTable->pfnSuggestGroupSize = zeKernelSuggestGroupSize_Tracing;
        if (nullptr == pDdiTable->pfnSuggestGroupSize) {
            pDdiTable->pfnSuggestGroupSize = driver_ddiTable.core_ddiTable.Kernel.pfnSuggestGroupSize;
        }
        pDdiTable->pfnSuggestMaxCooperativeGroupCount = zeKernelSuggestMaxCooperativeGroupCount_Tracing;
        if (nullptr == pDdiTable->pfnSuggestMaxCooperativeGroupCount) {
            pDdiTable->pfnSuggestMaxCooperativeGroupCount = driver_ddiTable.core_ddiTable.Kernel.pfnSuggestMaxCooperativeGroupCount;
        }
        pDdiTable->pfnSetArgumentValue = zeKernelSetArgumentValue_Tracing;
        if (nullptr == pDdiTable->pfnSetArgumentValue) {
            pDdiTable->pfnSetArgumentValue = driver_ddiTable.core_ddiTable.Kernel.pfnSetArgumentValue;
        }
        pDdiTable->pfnSetIndirectAccess = zeKernelSetIndirectAccess_Tracing;
        if (nullptr == pDdiTable->pfnSetIndirectAccess) {
            pDdiTable->pfnSetIndirectAccess = driver_ddiTable.core_ddiTable.Kernel.pfnSetIndirectAccess;
        }
        pDdiTable->pfnGetIndirectAccess = zeKernelGetIndirectAccess_Tracing;
        if (nullptr == pDdiTable->pfnGetIndirectAccess) {
            pDdiTable->pfnGetIndirectAccess = driver_ddiTable.core_ddiTable.Kernel.pfnGetIndirectAccess;
        }
        pDdiTable->pfnGetSourceAttributes = zeKernelGetSourceAttributes_Tracing;
        if (nullptr == pDdiTable->pfnGetSourceAttributes) {
            pDdiTable->pfnGetSourceAttributes = driver_ddiTable.core_ddiTable.Kernel.pfnGetSourceAttributes;
        }
        pDdiTable->pfnGetProperties = zeKernelGetProperties_Tracing;
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Kernel.pfnGetProperties;
        }
        pDdiTable->pfnSetCacheConfig = zeKernelSetCacheConfig_Tracing;
        if (nullptr == pDdiTable->pfnSetCacheConfig) {
            pDdiTable->pfnSetCacheConfig = driver_ddiTable.core_ddiTable.Kernel.pfnSetCacheConfig;
        }
        pDdiTable->pfnGetName = zeKernelGetName_Tracing;
        if (nullptr == pDdiTable->pfnGetName) {
            pDdiTable->pfnGetName = driver_ddiTable.core_ddiTable.Kernel.pfnGetName;
        }
    }
    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeGetSamplerProcAddrTable(
    ze_api_version_t version,
    ze_sampler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zeSamplerCreate;
    pDdiTable->pfnDestroy = zeSamplerDestroy;
    driver_ddiTable.core_ddiTable.Sampler = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = zeSamplerCreate_Tracing;
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Sampler.pfnCreate;
        }
        pDdiTable->pfnDestroy = zeSamplerDestroy_Tracing;
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Sampler.pfnDestroy;
        }
    }
    return result;
}
