/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/tracing/zet_tracing.h"
#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"
#include "zet_tools_all_api_entrypoints.h"

extern ze_gpu_driver_dditable_t driverDdiTable;

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetContextProcAddrTable(
    ze_api_version_t version,
    zet_context_dditable_t *pDdiTable) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    fillDdiEntry(pDdiTable->pfnActivateMetricGroups, L0::zetContextActivateMetricGroups, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricStreamerProcAddrTable(
    ze_api_version_t version,
    zet_metric_streamer_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnOpen, L0::zetMetricStreamerOpen, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnClose, L0::zetMetricStreamerClose, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadData, L0::zetMetricStreamerReadData, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetTracerExpProcAddrTable(
    ze_api_version_t version,
    zet_tracer_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zetTracerExpCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zetTracerExpDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetPrologues, L0::zetTracerExpSetPrologues, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetEpilogues, L0::zetTracerExpSetEpilogues, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetEnabled, L0::zetTracerExpSetEnabled, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetCommandListProcAddrTable(
    ze_api_version_t version,
    zet_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAppendMetricStreamerMarker, L0::zetCommandListAppendMetricStreamerMarker, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMetricQueryBegin, L0::zetCommandListAppendMetricQueryBegin, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMetricQueryEnd, L0::zetCommandListAppendMetricQueryEnd, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMetricMemoryBarrier, L0::zetCommandListAppendMetricMemoryBarrier, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetModuleProcAddrTable(
    ze_api_version_t version,
    zet_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetDebugInfo, L0::zetModuleGetDebugInfo, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetKernelProcAddrTable(
    ze_api_version_t version,
    zet_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetProfileInfo, L0::zetKernelGetProfileInfo, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricGroupProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::zetMetricGroupGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zetMetricGroupGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCalculateMetricValues, L0::zetMetricGroupCalculateMetricValues, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricProcAddrTable(
    ze_api_version_t version,
    zet_metric_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::zetMetricGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::zetMetricGetProperties, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryPoolProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zetMetricQueryPoolCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zetMetricQueryPoolDestroy, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::zetMetricQueryCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::zetMetricQueryDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::zetMetricQueryReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetData, L0::zetMetricQueryGetData, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDeviceProcAddrTable(
    ze_api_version_t version,
    zet_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetDebugProperties, L0::zetDeviceGetDebugProperties, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDebugProcAddrTable(
    ze_api_version_t version,
    zet_debug_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAttach, L0::zetDebugAttach, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDetach, L0::zetDebugDetach, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadEvent, L0::zetDebugReadEvent, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAcknowledgeEvent, L0::zetDebugAcknowledgeEvent, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnInterrupt, L0::zetDebugInterrupt, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnResume, L0::zetDebugResume, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadMemory, L0::zetDebugReadMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnWriteMemory, L0::zetDebugWriteMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetRegisterSetProperties, L0::zetDebugGetRegisterSetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadRegisters, L0::zetDebugReadRegisters, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnWriteRegisters, L0::zetDebugWriteRegisters, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetThreadRegisterSetProperties, L0::zetDebugGetThreadRegisterSetProperties, version, ZE_API_VERSION_1_5);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricGroupExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCalculateMultipleMetricValuesExp, L0::zetMetricGroupCalculateMultipleMetricValuesExp, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetGlobalTimestampsExp, L0::zetMetricGroupGetGlobalTimestampsExp, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetExportDataExp, L0::zetMetricGroupGetExportDataExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnCalculateMetricExportDataExp, L0::zetDriverCalculateMetricExportDataExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnCreateExp, L0::zetMetricGroupCreateExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnAddMetricExp, L0::zetMetricGroupAddMetricExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnRemoveMetricExp, L0::zetMetricGroupRemoveMetricExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnCloseExp, L0::zetMetricGroupCloseExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::zetMetricGroupDestroyExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricProgrammableExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_programmable_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetExp, L0::zetMetricProgrammableGetExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::zetMetricProgrammableGetPropertiesExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetParamInfoExp, L0::zetMetricProgrammableGetParamInfoExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetParamValueInfoExp, L0::zetMetricProgrammableGetParamValueInfoExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateFromProgrammableExp, L0::zetMetricCreateFromProgrammableExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::zetMetricDestroyExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricTracerExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_tracer_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnCreateExp, L0::zetMetricTracerCreateExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::zetMetricTracerDestroyExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnEnableExp, L0::zetMetricTracerEnableExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDisableExp, L0::zetMetricTracerDisableExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnReadDataExp, L0::zetMetricTracerReadDataExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDecodeExp, L0::zetMetricTracerDecodeExp, version, ZE_API_VERSION_1_11);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricDecoderExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_decoder_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnCreateExp, L0::zetMetricDecoderCreateExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::zetMetricDecoderDestroyExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnGetDecodableMetricsExp, L0::zetMetricDecoderGetDecodableMetricsExp, version, ZE_API_VERSION_1_11);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDeviceExpProcAddrTable(
    ze_api_version_t version,
    zet_device_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetConcurrentMetricGroupsExp, L0::zetDeviceGetConcurrentMetricGroupsExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnCreateMetricGroupsFromMetricsExp, L0::zetDeviceCreateMetricGroupsFromMetricsExp, version, ZE_API_VERSION_1_11);

    return result;
}