/*
 * Copyright (C) 2020-2021 Intel Corporation
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
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    pDdiTable->pfnActivateMetricGroups = zetContextActivateMetricGroups;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricStreamerProcAddrTable(
    ze_api_version_t version,
    zet_metric_streamer_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnOpen = zetMetricStreamerOpen;
    pDdiTable->pfnClose = zetMetricStreamerClose;
    pDdiTable->pfnReadData = zetMetricStreamerReadData;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetTracerExpProcAddrTable(
    ze_api_version_t version,
    zet_tracer_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zetTracerExpCreate;
    pDdiTable->pfnDestroy = zetTracerExpDestroy;
    pDdiTable->pfnSetPrologues = zetTracerExpSetPrologues;
    pDdiTable->pfnSetEpilogues = zetTracerExpSetEpilogues;
    pDdiTable->pfnSetEnabled = zetTracerExpSetEnabled;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetCommandListProcAddrTable(
    ze_api_version_t version,
    zet_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAppendMetricStreamerMarker = zetCommandListAppendMetricStreamerMarker;
    pDdiTable->pfnAppendMetricQueryBegin = zetCommandListAppendMetricQueryBegin;
    pDdiTable->pfnAppendMetricQueryEnd = zetCommandListAppendMetricQueryEnd;
    pDdiTable->pfnAppendMetricMemoryBarrier = zetCommandListAppendMetricMemoryBarrier;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetModuleProcAddrTable(
    ze_api_version_t version,
    zet_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetDebugInfo = zetModuleGetDebugInfo;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetKernelProcAddrTable(
    ze_api_version_t version,
    zet_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetProfileInfo = zetKernelGetProfileInfo;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricGroupProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = zetMetricGroupGet;
    pDdiTable->pfnGetProperties = zetMetricGroupGetProperties;
    pDdiTable->pfnCalculateMetricValues = zetMetricGroupCalculateMetricValues;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricProcAddrTable(
    ze_api_version_t version,
    zet_metric_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGet = zetMetricGet;
    pDdiTable->pfnGetProperties = zetMetricGetProperties;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryPoolProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zetMetricQueryPoolCreate;
    pDdiTable->pfnDestroy = zetMetricQueryPoolDestroy;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCreate = zetMetricQueryCreate;
    pDdiTable->pfnDestroy = zetMetricQueryDestroy;
    pDdiTable->pfnReset = zetMetricQueryReset;
    pDdiTable->pfnGetData = zetMetricQueryGetData;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDeviceProcAddrTable(
    ze_api_version_t version,
    zet_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnGetDebugProperties = zetDeviceGetDebugProperties;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDebugProcAddrTable(
    ze_api_version_t version,
    zet_debug_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnAttach = zetDebugAttach;
    pDdiTable->pfnDetach = zetDebugDetach;
    pDdiTable->pfnReadEvent = zetDebugReadEvent;
    pDdiTable->pfnAcknowledgeEvent = zetDebugAcknowledgeEvent;
    pDdiTable->pfnInterrupt = zetDebugInterrupt;
    pDdiTable->pfnResume = zetDebugResume;
    pDdiTable->pfnReadMemory = zetDebugReadMemory;
    pDdiTable->pfnWriteMemory = zetDebugWriteMemory;
    pDdiTable->pfnGetRegisterSetProperties = zetDebugGetRegisterSetProperties;
    pDdiTable->pfnReadRegisters = zetDebugReadRegisters;
    pDdiTable->pfnWriteRegisters = zetDebugWriteRegisters;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricGroupExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driver_ddiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driver_ddiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    pDdiTable->pfnCalculateMultipleMetricValuesExp = zetMetricGroupCalculateMultipleMetricValuesExp;

    return result;
}
