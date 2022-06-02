/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/tracing/zet_tracing.h"
#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/source/inc/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"
#include "zet_tools_all_api_entrypoints.h"

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
    pDdiTable->pfnActivateMetricGroups = L0::zetContextActivateMetricGroups;
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

    pDdiTable->pfnOpen = L0::zetMetricStreamerOpen;
    pDdiTable->pfnClose = L0::zetMetricStreamerClose;
    pDdiTable->pfnReadData = L0::zetMetricStreamerReadData;

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
    pDdiTable->pfnCreate = L0::zetTracerExpCreate;
    pDdiTable->pfnDestroy = L0::zetTracerExpDestroy;
    pDdiTable->pfnSetPrologues = L0::zetTracerExpSetPrologues;
    pDdiTable->pfnSetEpilogues = L0::zetTracerExpSetEpilogues;
    pDdiTable->pfnSetEnabled = L0::zetTracerExpSetEnabled;
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
    pDdiTable->pfnAppendMetricStreamerMarker = L0::zetCommandListAppendMetricStreamerMarker;
    pDdiTable->pfnAppendMetricQueryBegin = L0::zetCommandListAppendMetricQueryBegin;
    pDdiTable->pfnAppendMetricQueryEnd = L0::zetCommandListAppendMetricQueryEnd;
    pDdiTable->pfnAppendMetricMemoryBarrier = L0::zetCommandListAppendMetricMemoryBarrier;
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
    pDdiTable->pfnGetDebugInfo = L0::zetModuleGetDebugInfo;
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
    pDdiTable->pfnGetProfileInfo = L0::zetKernelGetProfileInfo;
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
    pDdiTable->pfnGet = L0::zetMetricGroupGet;
    pDdiTable->pfnGetProperties = L0::zetMetricGroupGetProperties;
    pDdiTable->pfnCalculateMetricValues = L0::zetMetricGroupCalculateMetricValues;
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
    pDdiTable->pfnGet = L0::zetMetricGet;
    pDdiTable->pfnGetProperties = L0::zetMetricGetProperties;
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
    pDdiTable->pfnCreate = L0::zetMetricQueryPoolCreate;
    pDdiTable->pfnDestroy = L0::zetMetricQueryPoolDestroy;
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
    pDdiTable->pfnCreate = L0::zetMetricQueryCreate;
    pDdiTable->pfnDestroy = L0::zetMetricQueryDestroy;
    pDdiTable->pfnReset = L0::zetMetricQueryReset;
    pDdiTable->pfnGetData = L0::zetMetricQueryGetData;
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
    pDdiTable->pfnGetDebugProperties = L0::zetDeviceGetDebugProperties;
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
    pDdiTable->pfnAttach = L0::zetDebugAttach;
    pDdiTable->pfnDetach = L0::zetDebugDetach;
    pDdiTable->pfnReadEvent = L0::zetDebugReadEvent;
    pDdiTable->pfnAcknowledgeEvent = L0::zetDebugAcknowledgeEvent;
    pDdiTable->pfnInterrupt = L0::zetDebugInterrupt;
    pDdiTable->pfnResume = L0::zetDebugResume;
    pDdiTable->pfnReadMemory = L0::zetDebugReadMemory;
    pDdiTable->pfnWriteMemory = L0::zetDebugWriteMemory;
    pDdiTable->pfnGetRegisterSetProperties = L0::zetDebugGetRegisterSetProperties;
    pDdiTable->pfnReadRegisters = L0::zetDebugReadRegisters;
    pDdiTable->pfnWriteRegisters = L0::zetDebugWriteRegisters;

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
    pDdiTable->pfnCalculateMultipleMetricValuesExp = L0::zetMetricGroupCalculateMultipleMetricValuesExp;

    return result;
}
