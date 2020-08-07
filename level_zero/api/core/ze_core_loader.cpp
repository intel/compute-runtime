/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = (ze_pfnDriverGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGet");
    pDdiTable->pfnGetApiVersion = (ze_pfnDriverGetApiVersion_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetApiVersion");
    pDdiTable->pfnGetProperties = (ze_pfnDriverGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetProperties");
    pDdiTable->pfnGetIpcProperties = (ze_pfnDriverGetIpcProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetIpcProperties");
    pDdiTable->pfnGetExtensionProperties = (ze_pfnDriverGetExtensionProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetExtensionProperties");
    driver_ddiTable.core_ddiTable.Driver = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGet = (ze_pfnDriverGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGet_Tracing");
        if (nullptr == pDdiTable->pfnGet) {
            pDdiTable->pfnGet = driver_ddiTable.core_ddiTable.Driver.pfnGet;
        }
        pDdiTable->pfnGetApiVersion = (ze_pfnDriverGetApiVersion_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetApiVersion_Tracing");
        if (nullptr == pDdiTable->pfnGetApiVersion) {
            pDdiTable->pfnGetApiVersion = driver_ddiTable.core_ddiTable.Driver.pfnGetApiVersion;
        }
        pDdiTable->pfnGetProperties = (ze_pfnDriverGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Driver.pfnGetProperties;
        }
        pDdiTable->pfnGetIpcProperties = (ze_pfnDriverGetIpcProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetIpcProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetIpcProperties) {
            pDdiTable->pfnGetIpcProperties = driver_ddiTable.core_ddiTable.Driver.pfnGetIpcProperties;
        }
        pDdiTable->pfnGetExtensionProperties = (ze_pfnDriverGetExtensionProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDriverGetExtensionProperties_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAllocShared = (ze_pfnMemAllocShared_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemAllocShared");
    pDdiTable->pfnAllocDevice = (ze_pfnMemAllocDevice_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemAllocDevice");
    pDdiTable->pfnAllocHost = (ze_pfnMemAllocHost_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemAllocHost");
    pDdiTable->pfnFree = (ze_pfnMemFree_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemFree");
    pDdiTable->pfnGetAllocProperties = (ze_pfnMemGetAllocProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemGetAllocProperties");
    pDdiTable->pfnGetAddressRange = (ze_pfnMemGetAddressRange_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemGetAddressRange");
    pDdiTable->pfnGetIpcHandle = (ze_pfnMemGetIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemGetIpcHandle");
    pDdiTable->pfnOpenIpcHandle = (ze_pfnMemOpenIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemOpenIpcHandle");
    pDdiTable->pfnCloseIpcHandle = (ze_pfnMemCloseIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemCloseIpcHandle");
    driver_ddiTable.core_ddiTable.Mem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnAllocShared = (ze_pfnMemAllocShared_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemAllocShared_Tracing");
        if (nullptr == pDdiTable->pfnAllocShared) {
            pDdiTable->pfnAllocShared = driver_ddiTable.core_ddiTable.Mem.pfnAllocShared;
        }
        pDdiTable->pfnAllocDevice = (ze_pfnMemAllocDevice_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemAllocDevice_Tracing");
        if (nullptr == pDdiTable->pfnAllocDevice) {
            pDdiTable->pfnAllocDevice = driver_ddiTable.core_ddiTable.Mem.pfnAllocDevice;
        }
        pDdiTable->pfnAllocHost = (ze_pfnMemAllocHost_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemAllocHost_Tracing");
        if (nullptr == pDdiTable->pfnAllocHost) {
            pDdiTable->pfnAllocHost = driver_ddiTable.core_ddiTable.Mem.pfnAllocHost;
        }
        pDdiTable->pfnFree = (ze_pfnMemFree_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemFree_Tracing");
        if (nullptr == pDdiTable->pfnAllocHost) {
            pDdiTable->pfnAllocHost = driver_ddiTable.core_ddiTable.Mem.pfnAllocHost;
        }
        pDdiTable->pfnGetAllocProperties = (ze_pfnMemGetAllocProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemGetAllocProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetAllocProperties) {
            pDdiTable->pfnGetAllocProperties = driver_ddiTable.core_ddiTable.Mem.pfnGetAllocProperties;
        }
        pDdiTable->pfnGetAddressRange = (ze_pfnMemGetAddressRange_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemGetAddressRange_Tracing");
        if (nullptr == pDdiTable->pfnGetAddressRange) {
            pDdiTable->pfnGetAddressRange = driver_ddiTable.core_ddiTable.Mem.pfnGetAddressRange;
        }
        pDdiTable->pfnGetIpcHandle = (ze_pfnMemGetIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemGetIpcHandle_Tracing");
        if (nullptr == pDdiTable->pfnGetIpcHandle) {
            pDdiTable->pfnGetIpcHandle = driver_ddiTable.core_ddiTable.Mem.pfnGetIpcHandle;
        }
        pDdiTable->pfnOpenIpcHandle = (ze_pfnMemOpenIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemOpenIpcHandle_Tracing");
        if (nullptr == pDdiTable->pfnOpenIpcHandle) {
            pDdiTable->pfnOpenIpcHandle = driver_ddiTable.core_ddiTable.Mem.pfnOpenIpcHandle;
        }
        pDdiTable->pfnCloseIpcHandle = (ze_pfnMemCloseIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeMemCloseIpcHandle_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnContextCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextCreate");
    pDdiTable->pfnDestroy = (ze_pfnContextDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextDestroy");
    pDdiTable->pfnGetStatus = (ze_pfnContextGetStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextGetStatus");
    pDdiTable->pfnSystemBarrier = (ze_pfnContextSystemBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextSystemBarrier");
    pDdiTable->pfnMakeMemoryResident = (ze_pfnContextMakeMemoryResident_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextMakeMemoryResident");
    pDdiTable->pfnEvictMemory = (ze_pfnContextEvictMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextEvictMemory");
    pDdiTable->pfnMakeImageResident = (ze_pfnContextMakeImageResident_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextMakeImageResident");
    pDdiTable->pfnEvictImage = (ze_pfnContextEvictImage_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextEvictImage");

    driver_ddiTable.core_ddiTable.Context = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnContextCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Context.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnContextDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Context.pfnDestroy;
        }
        pDdiTable->pfnGetStatus = (ze_pfnContextGetStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextGetStatus_Tracing");
        if (nullptr == pDdiTable->pfnGetStatus) {
            pDdiTable->pfnGetStatus = driver_ddiTable.core_ddiTable.Context.pfnGetStatus;
        }
        pDdiTable->pfnSystemBarrier = (ze_pfnContextSystemBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextSystemBarrier_Tracing");
        if (nullptr == pDdiTable->pfnSystemBarrier) {
            pDdiTable->pfnSystemBarrier = driver_ddiTable.core_ddiTable.Context.pfnSystemBarrier;
        }
        pDdiTable->pfnMakeMemoryResident = (ze_pfnContextMakeMemoryResident_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextMakeMemoryResident_Tracing");
        if (nullptr == pDdiTable->pfnMakeMemoryResident) {
            pDdiTable->pfnMakeMemoryResident = driver_ddiTable.core_ddiTable.Context.pfnMakeMemoryResident;
        }
        pDdiTable->pfnEvictMemory = (ze_pfnContextEvictMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextEvictMemory_Tracing");
        if (nullptr == pDdiTable->pfnEvictMemory) {
            pDdiTable->pfnEvictMemory = driver_ddiTable.core_ddiTable.Context.pfnEvictMemory;
        }
        pDdiTable->pfnMakeImageResident = (ze_pfnContextMakeImageResident_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextMakeImageResident_Tracing");
        if (nullptr == pDdiTable->pfnMakeImageResident) {
            pDdiTable->pfnMakeImageResident = driver_ddiTable.core_ddiTable.Context.pfnMakeImageResident;
        }
        pDdiTable->pfnEvictImage = (ze_pfnContextEvictImage_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeContextEvictImage_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnPhysicalMemCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zePhysicalMemCreate");
    pDdiTable->pfnDestroy = (ze_pfnPhysicalMemDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zePhysicalMemDestroy");

    driver_ddiTable.core_ddiTable.PhysicalMem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnPhysicalMemCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zePhysicalMemCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.PhysicalMem.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnPhysicalMemDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zePhysicalMemDestroy_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnReserve = (ze_pfnVirtualMemReserve_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemReserve");
    pDdiTable->pfnFree = (ze_pfnVirtualMemFree_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemFree");
    pDdiTable->pfnQueryPageSize = (ze_pfnVirtualMemQueryPageSize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemQueryPageSize");
    pDdiTable->pfnMap = (ze_pfnVirtualMemMap_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemMap ");
    pDdiTable->pfnUnmap = (ze_pfnVirtualMemUnmap_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemUnmap ");
    pDdiTable->pfnSetAccessAttribute = (ze_pfnVirtualMemSetAccessAttribute_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemSetAccessAttribute");
    pDdiTable->pfnGetAccessAttribute = (ze_pfnVirtualMemGetAccessAttribute_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemGetAccessAttribute");

    driver_ddiTable.core_ddiTable.VirtualMem = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnReserve = (ze_pfnVirtualMemReserve_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemReserve_Tracing");
        if (nullptr == pDdiTable->pfnReserve) {
            pDdiTable->pfnReserve = driver_ddiTable.core_ddiTable.VirtualMem.pfnReserve;
        }
        pDdiTable->pfnFree = (ze_pfnVirtualMemFree_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemFree_Tracing");
        if (nullptr == pDdiTable->pfnFree) {
            pDdiTable->pfnFree = driver_ddiTable.core_ddiTable.VirtualMem.pfnFree;
        }
        pDdiTable->pfnQueryPageSize = (ze_pfnVirtualMemQueryPageSize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemQueryPageSize_Tracing");
        if (nullptr == pDdiTable->pfnQueryPageSize) {
            pDdiTable->pfnQueryPageSize = driver_ddiTable.core_ddiTable.VirtualMem.pfnQueryPageSize;
        }
        pDdiTable->pfnMap = (ze_pfnVirtualMemMap_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemMap_Tracing");
        if (nullptr == pDdiTable->pfnMap) {
            pDdiTable->pfnMap = driver_ddiTable.core_ddiTable.VirtualMem.pfnMap;
        }
        pDdiTable->pfnUnmap = (ze_pfnVirtualMemUnmap_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemUnmap_Tracing");
        if (nullptr == pDdiTable->pfnUnmap) {
            pDdiTable->pfnUnmap = driver_ddiTable.core_ddiTable.VirtualMem.pfnUnmap;
        }
        pDdiTable->pfnSetAccessAttribute = (ze_pfnVirtualMemSetAccessAttribute_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemSetAccessAttribute_Tracing");
        if (nullptr == pDdiTable->pfnSetAccessAttribute) {
            pDdiTable->pfnSetAccessAttribute = driver_ddiTable.core_ddiTable.VirtualMem.pfnSetAccessAttribute;
        }
        pDdiTable->pfnGetAccessAttribute = (ze_pfnVirtualMemGetAccessAttribute_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeVirtualMemGetAccessAttribute_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnInit = (ze_pfnInit_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeInit");
    driver_ddiTable.core_ddiTable.Global = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnInit = (ze_pfnInit_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeInit_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = (ze_pfnDeviceGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGet");
    pDdiTable->pfnGetCommandQueueGroupProperties = (ze_pfnDeviceGetCommandQueueGroupProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetCommandQueueGroupProperties");
    pDdiTable->pfnGetSubDevices = (ze_pfnDeviceGetSubDevices_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetSubDevices");
    pDdiTable->pfnGetProperties = (ze_pfnDeviceGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetProperties");
    pDdiTable->pfnGetComputeProperties = (ze_pfnDeviceGetComputeProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetComputeProperties");
    pDdiTable->pfnGetModuleProperties = (ze_pfnDeviceGetModuleProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetModuleProperties");
    pDdiTable->pfnGetMemoryProperties = (ze_pfnDeviceGetMemoryProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetMemoryProperties");
    pDdiTable->pfnGetMemoryAccessProperties = (ze_pfnDeviceGetMemoryAccessProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetMemoryAccessProperties");
    pDdiTable->pfnGetCacheProperties = (ze_pfnDeviceGetCacheProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetCacheProperties");
    pDdiTable->pfnGetImageProperties = (ze_pfnDeviceGetImageProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetImageProperties");
    pDdiTable->pfnGetP2PProperties = (ze_pfnDeviceGetP2PProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetP2PProperties");
    pDdiTable->pfnCanAccessPeer = (ze_pfnDeviceCanAccessPeer_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceCanAccessPeer");
    pDdiTable->pfnGetStatus = (ze_pfnDeviceGetStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetStatus");
    pDdiTable->pfnGetExternalMemoryProperties = (ze_pfnDeviceGetExternalMemoryProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetExternalMemoryProperties");
    driver_ddiTable.core_ddiTable.Device = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGet = (ze_pfnDeviceGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGet_Tracing");
        if (nullptr == pDdiTable->pfnGet) {
            pDdiTable->pfnGet = driver_ddiTable.core_ddiTable.Device.pfnGet;
        }
        pDdiTable->pfnGetCommandQueueGroupProperties = (ze_pfnDeviceGetCommandQueueGroupProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetCommandQueueGroupProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetCommandQueueGroupProperties) {
            pDdiTable->pfnGetCommandQueueGroupProperties = driver_ddiTable.core_ddiTable.Device.pfnGetCommandQueueGroupProperties;
        }
        pDdiTable->pfnGetSubDevices = (ze_pfnDeviceGetSubDevices_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetSubDevices_Tracing");
        if (nullptr == pDdiTable->pfnGetSubDevices) {
            pDdiTable->pfnGetSubDevices = driver_ddiTable.core_ddiTable.Device.pfnGetSubDevices;
        }
        pDdiTable->pfnGetProperties = (ze_pfnDeviceGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Device.pfnGetProperties;
        }
        pDdiTable->pfnGetComputeProperties = (ze_pfnDeviceGetComputeProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetComputeProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetComputeProperties) {
            pDdiTable->pfnGetComputeProperties = driver_ddiTable.core_ddiTable.Device.pfnGetComputeProperties;
        }
        pDdiTable->pfnGetModuleProperties = (ze_pfnDeviceGetModuleProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetModuleProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetModuleProperties) {
            pDdiTable->pfnGetModuleProperties = driver_ddiTable.core_ddiTable.Device.pfnGetModuleProperties;
        }
        pDdiTable->pfnGetMemoryProperties = (ze_pfnDeviceGetMemoryProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetMemoryProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetMemoryProperties) {
            pDdiTable->pfnGetMemoryProperties = driver_ddiTable.core_ddiTable.Device.pfnGetMemoryProperties;
        }
        pDdiTable->pfnGetMemoryAccessProperties = (ze_pfnDeviceGetMemoryAccessProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetMemoryAccessProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetMemoryAccessProperties) {
            pDdiTable->pfnGetMemoryAccessProperties = driver_ddiTable.core_ddiTable.Device.pfnGetMemoryAccessProperties;
        }
        pDdiTable->pfnGetCacheProperties = (ze_pfnDeviceGetCacheProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetCacheProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetCacheProperties) {
            pDdiTable->pfnGetCacheProperties = driver_ddiTable.core_ddiTable.Device.pfnGetCacheProperties;
        }
        pDdiTable->pfnGetImageProperties = (ze_pfnDeviceGetImageProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetImageProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetImageProperties) {
            pDdiTable->pfnGetImageProperties = driver_ddiTable.core_ddiTable.Device.pfnGetImageProperties;
        }
        pDdiTable->pfnGetP2PProperties = (ze_pfnDeviceGetP2PProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetP2PProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetP2PProperties) {
            pDdiTable->pfnGetP2PProperties = driver_ddiTable.core_ddiTable.Device.pfnGetP2PProperties;
        }
        pDdiTable->pfnCanAccessPeer = (ze_pfnDeviceCanAccessPeer_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceCanAccessPeer_Tracing");
        if (nullptr == pDdiTable->pfnCanAccessPeer) {
            pDdiTable->pfnCanAccessPeer = driver_ddiTable.core_ddiTable.Device.pfnCanAccessPeer;
        }
        pDdiTable->pfnGetStatus = (ze_pfnDeviceGetStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetStatus_Tracing");
        if (nullptr == pDdiTable->pfnGetStatus) {
            pDdiTable->pfnGetStatus = driver_ddiTable.core_ddiTable.Device.pfnGetStatus;
        }
        pDdiTable->pfnGetExternalMemoryProperties = (ze_pfnDeviceGetExternalMemoryProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeDeviceGetExternalMemoryProperties_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnCommandQueueCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueCreate");
    pDdiTable->pfnDestroy = (ze_pfnCommandQueueDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueDestroy");
    pDdiTable->pfnExecuteCommandLists = (ze_pfnCommandQueueExecuteCommandLists_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueExecuteCommandLists");
    pDdiTable->pfnSynchronize = (ze_pfnCommandQueueSynchronize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueSynchronize");
    driver_ddiTable.core_ddiTable.CommandQueue = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnCommandQueueCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.CommandQueue.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnCommandQueueDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.CommandQueue.pfnDestroy;
        }
        pDdiTable->pfnExecuteCommandLists = (ze_pfnCommandQueueExecuteCommandLists_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueExecuteCommandLists_Tracing");
        if (nullptr == pDdiTable->pfnExecuteCommandLists) {
            pDdiTable->pfnExecuteCommandLists = driver_ddiTable.core_ddiTable.CommandQueue.pfnExecuteCommandLists;
        }
        pDdiTable->pfnSynchronize = (ze_pfnCommandQueueSynchronize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandQueueSynchronize_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAppendBarrier = (ze_pfnCommandListAppendBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendBarrier");
    pDdiTable->pfnAppendMemoryRangesBarrier = (ze_pfnCommandListAppendMemoryRangesBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryRangesBarrier");
    pDdiTable->pfnCreate = (ze_pfnCommandListCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListCreate");
    pDdiTable->pfnCreateImmediate = (ze_pfnCommandListCreateImmediate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListCreateImmediate");
    pDdiTable->pfnDestroy = (ze_pfnCommandListDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListDestroy");
    pDdiTable->pfnClose = (ze_pfnCommandListClose_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListClose");
    pDdiTable->pfnReset = (ze_pfnCommandListReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListReset");
    pDdiTable->pfnAppendMemoryCopy = (ze_pfnCommandListAppendMemoryCopy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryCopy");
    pDdiTable->pfnAppendMemoryCopyRegion = (ze_pfnCommandListAppendMemoryCopyRegion_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryCopyRegion");
    pDdiTable->pfnAppendMemoryFill = (ze_pfnCommandListAppendMemoryFill_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryFill");
    pDdiTable->pfnAppendImageCopy = (ze_pfnCommandListAppendImageCopy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopy");
    pDdiTable->pfnAppendImageCopyRegion = (ze_pfnCommandListAppendImageCopyRegion_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopyRegion");
    pDdiTable->pfnAppendImageCopyToMemory = (ze_pfnCommandListAppendImageCopyToMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopyToMemory");
    pDdiTable->pfnAppendImageCopyFromMemory = (ze_pfnCommandListAppendImageCopyFromMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopyFromMemory");
    pDdiTable->pfnAppendMemoryPrefetch = (ze_pfnCommandListAppendMemoryPrefetch_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryPrefetch");
    pDdiTable->pfnAppendMemAdvise = (ze_pfnCommandListAppendMemAdvise_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemAdvise");
    pDdiTable->pfnAppendSignalEvent = (ze_pfnCommandListAppendSignalEvent_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendSignalEvent");
    pDdiTable->pfnAppendWaitOnEvents = (ze_pfnCommandListAppendWaitOnEvents_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendWaitOnEvents");
    pDdiTable->pfnAppendEventReset = (ze_pfnCommandListAppendEventReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendEventReset");
    pDdiTable->pfnAppendLaunchKernel = (ze_pfnCommandListAppendLaunchKernel_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchKernel");
    pDdiTable->pfnAppendLaunchCooperativeKernel = (ze_pfnCommandListAppendLaunchCooperativeKernel_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchCooperativeKernel");
    pDdiTable->pfnAppendLaunchKernelIndirect = (ze_pfnCommandListAppendLaunchKernelIndirect_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchKernelIndirect");
    pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = (ze_pfnCommandListAppendLaunchMultipleKernelsIndirect_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchMultipleKernelsIndirect");
    pDdiTable->pfnAppendWriteGlobalTimestamp = (ze_pfnCommandListAppendWriteGlobalTimestamp_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendWriteGlobalTimestamp");
    pDdiTable->pfnAppendMemoryCopyFromContext = (ze_pfnCommandListAppendMemoryCopyFromContext_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryCopyFromContext");
    pDdiTable->pfnAppendQueryKernelTimestamps = (ze_pfnCommandListAppendQueryKernelTimestamps_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendQueryKernelTimestamps");
    driver_ddiTable.core_ddiTable.CommandList = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnAppendBarrier = (ze_pfnCommandListAppendBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendBarrier_Tracing");
        if (nullptr == pDdiTable->pfnAppendBarrier) {
            pDdiTable->pfnAppendBarrier = driver_ddiTable.core_ddiTable.CommandList.pfnAppendBarrier;
        }
        pDdiTable->pfnAppendMemoryRangesBarrier = (ze_pfnCommandListAppendMemoryRangesBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryRangesBarrier_Tracing");
        if (nullptr == pDdiTable->pfnAppendMemoryRangesBarrier) {
            pDdiTable->pfnAppendMemoryRangesBarrier = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryRangesBarrier;
        }
        pDdiTable->pfnCreate = (ze_pfnCommandListCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.CommandList.pfnCreate;
        }
        pDdiTable->pfnCreateImmediate = (ze_pfnCommandListCreateImmediate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListCreateImmediate_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.CommandList.pfnDestroy;
        }
        pDdiTable->pfnDestroy = (ze_pfnCommandListDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.CommandList.pfnDestroy;
        }
        pDdiTable->pfnClose = (ze_pfnCommandListClose_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListClose_Tracing");
        if (nullptr == pDdiTable->pfnClose) {
            pDdiTable->pfnClose = driver_ddiTable.core_ddiTable.CommandList.pfnClose;
        }
        pDdiTable->pfnReset = (ze_pfnCommandListReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListReset_Tracing");
        if (nullptr == pDdiTable->pfnReset) {
            pDdiTable->pfnReset = driver_ddiTable.core_ddiTable.CommandList.pfnReset;
        }
        pDdiTable->pfnAppendMemoryCopy = (ze_pfnCommandListAppendMemoryCopy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryCopy_Tracing");
        if (nullptr == pDdiTable->pfnAppendMemoryCopy) {
            pDdiTable->pfnAppendMemoryCopy = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopy;
        }
        pDdiTable->pfnAppendMemoryCopyRegion = (ze_pfnCommandListAppendMemoryCopyRegion_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryCopyRegion_Tracing");
        if (nullptr == pDdiTable->pfnAppendMemoryCopyRegion) {
            pDdiTable->pfnAppendMemoryCopyRegion = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyRegion;
        }
        pDdiTable->pfnAppendMemoryFill = (ze_pfnCommandListAppendMemoryFill_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryFill_Tracing");
        if (nullptr == pDdiTable->pfnAppendMemoryFill) {
            pDdiTable->pfnAppendMemoryFill = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryFill;
        }
        pDdiTable->pfnAppendImageCopy = (ze_pfnCommandListAppendImageCopy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopy_Tracing");
        if (nullptr == pDdiTable->pfnAppendImageCopy) {
            pDdiTable->pfnAppendImageCopy = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopy;
        }
        pDdiTable->pfnAppendImageCopyRegion = (ze_pfnCommandListAppendImageCopyRegion_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopyRegion_Tracing");
        if (nullptr == pDdiTable->pfnAppendImageCopyRegion) {
            pDdiTable->pfnAppendImageCopyRegion = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyRegion;
        }
        pDdiTable->pfnAppendImageCopyToMemory = (ze_pfnCommandListAppendImageCopyToMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopyToMemory_Tracing");
        if (nullptr == pDdiTable->pfnAppendImageCopyToMemory) {
            pDdiTable->pfnAppendImageCopyToMemory = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyToMemory;
        }
        pDdiTable->pfnAppendImageCopyFromMemory = (ze_pfnCommandListAppendImageCopyFromMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendImageCopyFromMemory_Tracing");
        if (nullptr == pDdiTable->pfnAppendImageCopyFromMemory) {
            pDdiTable->pfnAppendImageCopyFromMemory = driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyFromMemory;
        }
        pDdiTable->pfnAppendMemoryPrefetch = (ze_pfnCommandListAppendMemoryPrefetch_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryPrefetch_Tracing");
        if (nullptr == pDdiTable->pfnAppendMemoryPrefetch) {
            pDdiTable->pfnAppendMemoryPrefetch = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryPrefetch;
        }
        pDdiTable->pfnAppendMemAdvise = (ze_pfnCommandListAppendMemAdvise_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemAdvise_Tracing");
        if (nullptr == pDdiTable->pfnAppendMemAdvise) {
            pDdiTable->pfnAppendMemAdvise = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemAdvise;
        }
        pDdiTable->pfnAppendSignalEvent = (ze_pfnCommandListAppendSignalEvent_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendSignalEvent_Tracing");
        if (nullptr == pDdiTable->pfnAppendSignalEvent) {
            pDdiTable->pfnAppendSignalEvent = driver_ddiTable.core_ddiTable.CommandList.pfnAppendSignalEvent;
        }
        pDdiTable->pfnAppendWaitOnEvents = (ze_pfnCommandListAppendWaitOnEvents_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendWaitOnEvents_Tracing");
        if (nullptr == pDdiTable->pfnAppendWaitOnEvents) {
            pDdiTable->pfnAppendWaitOnEvents = driver_ddiTable.core_ddiTable.CommandList.pfnAppendWaitOnEvents;
        }
        pDdiTable->pfnAppendEventReset = (ze_pfnCommandListAppendEventReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendEventReset_Tracing");
        if (nullptr == pDdiTable->pfnAppendEventReset) {
            pDdiTable->pfnAppendEventReset = driver_ddiTable.core_ddiTable.CommandList.pfnAppendEventReset;
        }
        pDdiTable->pfnAppendLaunchKernel = (ze_pfnCommandListAppendLaunchKernel_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchKernel_Tracing");
        if (nullptr == pDdiTable->pfnAppendLaunchKernel) {
            pDdiTable->pfnAppendLaunchKernel = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernel;
        }
        pDdiTable->pfnAppendLaunchCooperativeKernel = (ze_pfnCommandListAppendLaunchCooperativeKernel_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchCooperativeKernel_Tracing");
        if (nullptr == pDdiTable->pfnAppendLaunchCooperativeKernel) {
            pDdiTable->pfnAppendLaunchCooperativeKernel = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchCooperativeKernel;
        }
        pDdiTable->pfnAppendLaunchKernelIndirect = (ze_pfnCommandListAppendLaunchKernelIndirect_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchKernelIndirect_Tracing");
        if (nullptr == pDdiTable->pfnAppendLaunchKernelIndirect) {
            pDdiTable->pfnAppendLaunchKernelIndirect = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernelIndirect;
        }
        pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = (ze_pfnCommandListAppendLaunchMultipleKernelsIndirect_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendLaunchMultipleKernelsIndirect_Tracing");
        if (nullptr == pDdiTable->pfnAppendLaunchMultipleKernelsIndirect) {
            pDdiTable->pfnAppendLaunchMultipleKernelsIndirect = driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchMultipleKernelsIndirect;
        }
        pDdiTable->pfnAppendWriteGlobalTimestamp = (ze_pfnCommandListAppendWriteGlobalTimestamp_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendWriteGlobalTimestamp_Tracing");
        if (nullptr == pDdiTable->pfnAppendWriteGlobalTimestamp) {
            pDdiTable->pfnAppendWriteGlobalTimestamp = driver_ddiTable.core_ddiTable.CommandList.pfnAppendWriteGlobalTimestamp;
        }
        pDdiTable->pfnAppendMemoryCopyFromContext = (ze_pfnCommandListAppendMemoryCopyFromContext_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendMemoryCopyFromContext_Tracing");
        if (nullptr == pDdiTable->pfnAppendMemoryCopyFromContext) {
            pDdiTable->pfnAppendMemoryCopyFromContext = driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyFromContext;
        }
        pDdiTable->pfnAppendQueryKernelTimestamps = (ze_pfnCommandListAppendQueryKernelTimestamps_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeCommandListAppendQueryKernelTimestamps_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnFenceCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceCreate");
    pDdiTable->pfnDestroy = (ze_pfnFenceDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceDestroy");
    pDdiTable->pfnHostSynchronize = (ze_pfnFenceHostSynchronize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceHostSynchronize");
    pDdiTable->pfnQueryStatus = (ze_pfnFenceQueryStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceQueryStatus");
    pDdiTable->pfnReset = (ze_pfnFenceReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceReset");
    driver_ddiTable.core_ddiTable.Fence = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnFenceCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Fence.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnFenceDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Fence.pfnDestroy;
        }
        pDdiTable->pfnHostSynchronize = (ze_pfnFenceHostSynchronize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceHostSynchronize_Tracing");
        if (nullptr == pDdiTable->pfnHostSynchronize) {
            pDdiTable->pfnHostSynchronize = driver_ddiTable.core_ddiTable.Fence.pfnHostSynchronize;
        }
        pDdiTable->pfnQueryStatus = (ze_pfnFenceQueryStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceQueryStatus_Tracing");
        if (nullptr == pDdiTable->pfnQueryStatus) {
            pDdiTable->pfnQueryStatus = driver_ddiTable.core_ddiTable.Fence.pfnQueryStatus;
        }
        pDdiTable->pfnReset = (ze_pfnFenceReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeFenceReset_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnEventPoolCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolCreate");
    pDdiTable->pfnDestroy = (ze_pfnEventPoolDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolDestroy");
    pDdiTable->pfnGetIpcHandle = (ze_pfnEventPoolGetIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolGetIpcHandle");
    pDdiTable->pfnOpenIpcHandle = (ze_pfnEventPoolOpenIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolOpenIpcHandle");
    pDdiTable->pfnCloseIpcHandle = (ze_pfnEventPoolCloseIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolCloseIpcHandle");
    driver_ddiTable.core_ddiTable.EventPool = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnEventPoolCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.EventPool.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnEventPoolDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.EventPool.pfnDestroy;
        }
        pDdiTable->pfnGetIpcHandle = (ze_pfnEventPoolGetIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolGetIpcHandle_Tracing");
        if (nullptr == pDdiTable->pfnGetIpcHandle) {
            pDdiTable->pfnGetIpcHandle = driver_ddiTable.core_ddiTable.EventPool.pfnGetIpcHandle;
        }
        pDdiTable->pfnOpenIpcHandle = (ze_pfnEventPoolOpenIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolOpenIpcHandle_Tracing");
        if (nullptr == pDdiTable->pfnOpenIpcHandle) {
            pDdiTable->pfnOpenIpcHandle = driver_ddiTable.core_ddiTable.EventPool.pfnOpenIpcHandle;
        }
        pDdiTable->pfnCloseIpcHandle = (ze_pfnEventPoolCloseIpcHandle_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventPoolCloseIpcHandle_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnEventCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventCreate");
    pDdiTable->pfnDestroy = (ze_pfnEventDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventDestroy");
    pDdiTable->pfnHostSignal = (ze_pfnEventHostSignal_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventHostSignal");
    pDdiTable->pfnHostSynchronize = (ze_pfnEventHostSynchronize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventHostSynchronize");
    pDdiTable->pfnQueryStatus = (ze_pfnEventQueryStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventQueryStatus");
    pDdiTable->pfnHostReset = (ze_pfnEventHostReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventHostReset");
    pDdiTable->pfnQueryKernelTimestamp = (ze_pfnEventQueryKernelTimestamp_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventQueryKernelTimestamp");
    driver_ddiTable.core_ddiTable.Event = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnEventCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Event.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnEventDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Event.pfnDestroy;
        }
        pDdiTable->pfnHostSignal = (ze_pfnEventHostSignal_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventHostSignal_Tracing");
        if (nullptr == pDdiTable->pfnHostSignal) {
            pDdiTable->pfnHostSignal = driver_ddiTable.core_ddiTable.Event.pfnHostSignal;
        }
        pDdiTable->pfnHostSynchronize = (ze_pfnEventHostSynchronize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventHostSynchronize_Tracing");
        if (nullptr == pDdiTable->pfnHostSynchronize) {
            pDdiTable->pfnHostSynchronize = driver_ddiTable.core_ddiTable.Event.pfnHostSynchronize;
        }
        pDdiTable->pfnQueryStatus = (ze_pfnEventQueryStatus_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventQueryStatus_Tracing");
        if (nullptr == pDdiTable->pfnQueryStatus) {
            pDdiTable->pfnQueryStatus = driver_ddiTable.core_ddiTable.Event.pfnQueryStatus;
        }
        pDdiTable->pfnHostReset = (ze_pfnEventHostReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventHostReset_Tracing");
        if (nullptr == pDdiTable->pfnHostReset) {
            pDdiTable->pfnHostReset = driver_ddiTable.core_ddiTable.Event.pfnHostReset;
        }
        pDdiTable->pfnQueryKernelTimestamp = (ze_pfnEventQueryKernelTimestamp_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeEventQueryKernelTimestamp_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProperties = (ze_pfnImageGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeImageGetProperties");
    pDdiTable->pfnCreate = (ze_pfnImageCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeImageCreate");
    pDdiTable->pfnDestroy = (ze_pfnImageDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeImageDestroy");
    driver_ddiTable.core_ddiTable.Image = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnGetProperties = (ze_pfnImageGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeImageGetProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Image.pfnGetProperties;
        }
        pDdiTable->pfnCreate = (ze_pfnImageCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeImageCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Image.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnImageDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeImageDestroy_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnModuleCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleCreate");
    pDdiTable->pfnDestroy = (ze_pfnModuleDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleDestroy");
    pDdiTable->pfnDynamicLink = (ze_pfnModuleDynamicLink_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleDynamicLink");
    pDdiTable->pfnGetNativeBinary = (ze_pfnModuleGetNativeBinary_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetNativeBinary");
    pDdiTable->pfnGetGlobalPointer = (ze_pfnModuleGetGlobalPointer_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetGlobalPointer");
    pDdiTable->pfnGetKernelNames = (ze_pfnModuleGetKernelNames_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetKernelNames");
    pDdiTable->pfnGetFunctionPointer = (ze_pfnModuleGetFunctionPointer_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetFunctionPointer");
    pDdiTable->pfnGetProperties = (ze_pfnModuleGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetProperties");
    driver_ddiTable.core_ddiTable.Module = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnModuleCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Module.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnModuleDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Module.pfnDestroy;
        }
        pDdiTable->pfnGetNativeBinary = (ze_pfnModuleGetNativeBinary_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetNativeBinary_Tracing");
        if (nullptr == pDdiTable->pfnGetNativeBinary) {
            pDdiTable->pfnGetNativeBinary = driver_ddiTable.core_ddiTable.Module.pfnGetNativeBinary;
        }
        pDdiTable->pfnDynamicLink = (ze_pfnModuleDynamicLink_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleDynamicLink_Tracing");
        if (nullptr == pDdiTable->pfnDynamicLink) {
            pDdiTable->pfnDynamicLink = driver_ddiTable.core_ddiTable.Module.pfnDynamicLink;
        }
        pDdiTable->pfnGetGlobalPointer = (ze_pfnModuleGetGlobalPointer_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetGlobalPointer_Tracing");
        if (nullptr == pDdiTable->pfnGetGlobalPointer) {
            pDdiTable->pfnGetGlobalPointer = driver_ddiTable.core_ddiTable.Module.pfnGetGlobalPointer;
        }
        pDdiTable->pfnGetFunctionPointer = (ze_pfnModuleGetFunctionPointer_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetFunctionPointer_Tracing");
        if (nullptr == pDdiTable->pfnGetFunctionPointer) {
            pDdiTable->pfnGetFunctionPointer = driver_ddiTable.core_ddiTable.Module.pfnGetFunctionPointer;
        }
        pDdiTable->pfnGetKernelNames = (ze_pfnModuleGetKernelNames_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetKernelNames_Tracing");
        if (nullptr == pDdiTable->pfnGetKernelNames) {
            pDdiTable->pfnGetKernelNames = driver_ddiTable.core_ddiTable.Module.pfnGetKernelNames;
        }
        pDdiTable->pfnGetProperties = (ze_pfnModuleGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleGetProperties_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnDestroy = (ze_pfnModuleBuildLogDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleBuildLogDestroy");
    pDdiTable->pfnGetString = (ze_pfnModuleBuildLogGetString_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleBuildLogGetString");
    driver_ddiTable.core_ddiTable.ModuleBuildLog = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnDestroy = (ze_pfnModuleBuildLogDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleBuildLogDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnDestroy;
        }
        pDdiTable->pfnGetString = (ze_pfnModuleBuildLogGetString_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeModuleBuildLogGetString_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnKernelCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelCreate");
    pDdiTable->pfnDestroy = (ze_pfnKernelDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelDestroy");
    pDdiTable->pfnSetGroupSize = (ze_pfnKernelSetGroupSize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetGroupSize");
    pDdiTable->pfnSuggestGroupSize = (ze_pfnKernelSuggestGroupSize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSuggestGroupSize");
    pDdiTable->pfnSuggestMaxCooperativeGroupCount = (ze_pfnKernelSuggestMaxCooperativeGroupCount_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSuggestMaxCooperativeGroupCount");
    pDdiTable->pfnSetArgumentValue = (ze_pfnKernelSetArgumentValue_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetArgumentValue");
    pDdiTable->pfnSetIndirectAccess = (ze_pfnKernelSetIndirectAccess_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetIndirectAccess");
    pDdiTable->pfnGetIndirectAccess = (ze_pfnKernelGetIndirectAccess_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetIndirectAccess");
    pDdiTable->pfnGetSourceAttributes = (ze_pfnKernelGetSourceAttributes_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetSourceAttributes");
    pDdiTable->pfnGetProperties = (ze_pfnKernelGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetProperties");
    pDdiTable->pfnSetCacheConfig = (ze_pfnKernelSetCacheConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetCacheConfig");
    pDdiTable->pfnGetName = (ze_pfnKernelGetName_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetName");
    driver_ddiTable.core_ddiTable.Kernel = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnKernelCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Kernel.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnKernelDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Kernel.pfnDestroy;
        }
        pDdiTable->pfnSetGroupSize = (ze_pfnKernelSetGroupSize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetGroupSize_Tracing");
        if (nullptr == pDdiTable->pfnSetGroupSize) {
            pDdiTable->pfnSetGroupSize = driver_ddiTable.core_ddiTable.Kernel.pfnSetGroupSize;
        }
        pDdiTable->pfnSuggestGroupSize = (ze_pfnKernelSuggestGroupSize_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSuggestGroupSize_Tracing");
        if (nullptr == pDdiTable->pfnSuggestGroupSize) {
            pDdiTable->pfnSuggestGroupSize = driver_ddiTable.core_ddiTable.Kernel.pfnSuggestGroupSize;
        }
        pDdiTable->pfnSuggestMaxCooperativeGroupCount = (ze_pfnKernelSuggestMaxCooperativeGroupCount_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSuggestMaxCooperativeGroupCount_Tracing");
        if (nullptr == pDdiTable->pfnSuggestMaxCooperativeGroupCount) {
            pDdiTable->pfnSuggestMaxCooperativeGroupCount = driver_ddiTable.core_ddiTable.Kernel.pfnSuggestMaxCooperativeGroupCount;
        }
        pDdiTable->pfnSetArgumentValue = (ze_pfnKernelSetArgumentValue_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetArgumentValue_Tracing");
        if (nullptr == pDdiTable->pfnSetArgumentValue) {
            pDdiTable->pfnSetArgumentValue = driver_ddiTable.core_ddiTable.Kernel.pfnSetArgumentValue;
        }
        pDdiTable->pfnSetIndirectAccess = (ze_pfnKernelSetIndirectAccess_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetIndirectAccess_Tracing");
        if (nullptr == pDdiTable->pfnSetIndirectAccess) {
            pDdiTable->pfnSetIndirectAccess = driver_ddiTable.core_ddiTable.Kernel.pfnSetIndirectAccess;
        }
        pDdiTable->pfnGetIndirectAccess = (ze_pfnKernelGetIndirectAccess_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetIndirectAccess_Tracing");
        if (nullptr == pDdiTable->pfnGetIndirectAccess) {
            pDdiTable->pfnGetIndirectAccess = driver_ddiTable.core_ddiTable.Kernel.pfnGetIndirectAccess;
        }
        pDdiTable->pfnGetSourceAttributes = (ze_pfnKernelGetSourceAttributes_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetSourceAttributes_Tracing");
        if (nullptr == pDdiTable->pfnGetSourceAttributes) {
            pDdiTable->pfnGetSourceAttributes = driver_ddiTable.core_ddiTable.Kernel.pfnGetSourceAttributes;
        }
        pDdiTable->pfnGetProperties = (ze_pfnKernelGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetProperties_Tracing");
        if (nullptr == pDdiTable->pfnGetProperties) {
            pDdiTable->pfnGetProperties = driver_ddiTable.core_ddiTable.Kernel.pfnGetProperties;
        }
        pDdiTable->pfnSetCacheConfig = (ze_pfnKernelSetCacheConfig_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelSetCacheConfig_Tracing");
        if (nullptr == pDdiTable->pfnSetCacheConfig) {
            pDdiTable->pfnSetCacheConfig = driver_ddiTable.core_ddiTable.Kernel.pfnSetCacheConfig;
        }
        pDdiTable->pfnGetName = (ze_pfnKernelGetName_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeKernelGetName_Tracing");
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
    if (nullptr == driver_ddiTable.driverLibrary) {
        driver_ddiTable.driverLibrary = LOAD_INTEL_GPU_LIBRARY();
        if (nullptr == driver_ddiTable.driverLibrary) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        driver_ddiTable.enableTracing = getenv_tobool("ZET_ENABLE_API_TRACING_EXP");
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (ze_pfnSamplerCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeSamplerCreate");
    pDdiTable->pfnDestroy = (ze_pfnSamplerDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeSamplerDestroy");
    driver_ddiTable.core_ddiTable.Sampler = *pDdiTable;
    if (driver_ddiTable.enableTracing) {
        pDdiTable->pfnCreate = (ze_pfnSamplerCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeSamplerCreate_Tracing");
        if (nullptr == pDdiTable->pfnCreate) {
            pDdiTable->pfnCreate = driver_ddiTable.core_ddiTable.Sampler.pfnCreate;
        }
        pDdiTable->pfnDestroy = (ze_pfnSamplerDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, "zeSamplerDestroy_Tracing");
        if (nullptr == pDdiTable->pfnDestroy) {
            pDdiTable->pfnDestroy = driver_ddiTable.core_ddiTable.Sampler.pfnDestroy;
        }
    }
    return result;
}
