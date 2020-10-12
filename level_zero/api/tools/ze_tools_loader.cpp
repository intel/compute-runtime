/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/source/inc/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"

extern ze_gpu_driver_dditable_t driver_ddiTable;

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetContextProcAddrTable(
    ze_api_version_t version,
    zet_context_dditable_t *pDdiTable) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    pDdiTable->pfnActivateMetricGroups = (zet_pfnContextActivateMetricGroups_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetContextActivateMetricGroups);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricStreamerProcAddrTable(
    ze_api_version_t version,
    zet_metric_streamer_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnOpen = (zet_pfnMetricStreamerOpen_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricStreamerOpen);
    pDdiTable->pfnClose = (zet_pfnMetricStreamerClose_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricStreamerClose);
    pDdiTable->pfnReadData = (zet_pfnMetricStreamerReadData_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricStreamerReadData);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetTracerExpProcAddrTable(
    ze_api_version_t version,
    zet_tracer_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (zet_pfnTracerExpCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetTracerExpCreate);
    pDdiTable->pfnDestroy = (zet_pfnTracerExpDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetTracerExpDestroy);
    pDdiTable->pfnSetPrologues = (zet_pfnTracerExpSetPrologues_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetTracerExpSetPrologues);
    pDdiTable->pfnSetEpilogues = (zet_pfnTracerExpSetEpilogues_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetTracerExpSetEpilogues);
    pDdiTable->pfnSetEnabled = (zet_pfnTracerExpSetEnabled_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetTracerExpSetEnabled);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetCommandListProcAddrTable(
    ze_api_version_t version,
    zet_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAppendMetricStreamerMarker = (zet_pfnCommandListAppendMetricStreamerMarker_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetCommandListAppendMetricStreamerMarker);
    pDdiTable->pfnAppendMetricQueryBegin = (zet_pfnCommandListAppendMetricQueryBegin_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetCommandListAppendMetricQueryBegin);
    pDdiTable->pfnAppendMetricQueryEnd = (zet_pfnCommandListAppendMetricQueryEnd_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetCommandListAppendMetricQueryEnd);
    pDdiTable->pfnAppendMetricMemoryBarrier = (zet_pfnCommandListAppendMetricMemoryBarrier_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetCommandListAppendMetricMemoryBarrier);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetModuleProcAddrTable(
    ze_api_version_t version,
    zet_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetDebugInfo = (zet_pfnModuleGetDebugInfo_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetModuleGetDebugInfo);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetKernelProcAddrTable(
    ze_api_version_t version,
    zet_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProfileInfo = (zet_pfnKernelGetProfileInfo_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetKernelGetProfileInfo);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricGroupProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = (zet_pfnMetricGroupGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricGroupGet);
    pDdiTable->pfnGetProperties = (zet_pfnMetricGroupGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricGroupGetProperties);
    pDdiTable->pfnCalculateMetricValues = (zet_pfnMetricGroupCalculateMetricValues_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricGroupCalculateMetricValues);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricProcAddrTable(
    ze_api_version_t version,
    zet_metric_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = (zet_pfnMetricGet_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricGet);
    pDdiTable->pfnGetProperties = (zet_pfnMetricGetProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricGetProperties);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryPoolProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (zet_pfnMetricQueryPoolCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricQueryPoolCreate);
    pDdiTable->pfnDestroy = (zet_pfnMetricQueryPoolDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricQueryPoolDestroy);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = (zet_pfnMetricQueryCreate_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricQueryCreate);
    pDdiTable->pfnDestroy = (zet_pfnMetricQueryDestroy_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricQueryDestroy);
    pDdiTable->pfnReset = (zet_pfnMetricQueryReset_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricQueryReset);
    pDdiTable->pfnGetData = (zet_pfnMetricQueryGetData_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetMetricQueryGetData);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDeviceProcAddrTable(
    ze_api_version_t version,
    zet_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetDebugProperties = (zet_pfnDeviceGetDebugProperties_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDeviceGetDebugProperties);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDebugProcAddrTable(
    ze_api_version_t version,
    zet_debug_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (driver_ddiTable.version < version)
        return ZE_RESULT_ERROR_UNKNOWN;
    if (nullptr == driver_ddiTable.driverLibrary) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAttach = (zet_pfnDebugAttach_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDebugAttach);
    pDdiTable->pfnDetach = (zet_pfnDebugDetach_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDebugDetach);
    pDdiTable->pfnReadEvent = (zet_pfnDebugReadEvent_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDebugReadEvent);
    pDdiTable->pfnInterrupt = (zet_pfnDebugInterrupt_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDebugInterrupt);
    pDdiTable->pfnResume = (zet_pfnDebugResume_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDebugResume);
    pDdiTable->pfnReadMemory = (zet_pfnDebugReadMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDebugReadMemory);
    pDdiTable->pfnWriteMemory = (zet_pfnDebugWriteMemory_t)GET_FUNCTION_PTR(driver_ddiTable.driverLibrary, zetDebugWriteMemory);

    return result;
}
