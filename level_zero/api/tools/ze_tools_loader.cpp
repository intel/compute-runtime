/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetContextProcAddrTable(
    ze_api_version_t version,
    zet_context_dditable_t *pDdiTable) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    fillDdiEntry(pDdiTable->pfnActivateMetricGroups, L0::globalDriverDispatch.toolsContext.pfnActivateMetricGroups, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricStreamerProcAddrTable(
    ze_api_version_t version,
    zet_metric_streamer_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnOpen, L0::globalDriverDispatch.toolsMetricStreamer.pfnOpen, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnClose, L0::globalDriverDispatch.toolsMetricStreamer.pfnClose, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadData, L0::globalDriverDispatch.toolsMetricStreamer.pfnReadData, version, ZE_API_VERSION_1_0);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetTracerExpProcAddrTable(
    ze_api_version_t version,
    zet_tracer_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.toolsTracerExp.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.toolsTracerExp.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetPrologues, L0::globalDriverDispatch.toolsTracerExp.pfnSetPrologues, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetEpilogues, L0::globalDriverDispatch.toolsTracerExp.pfnSetEpilogues, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnSetEnabled, L0::globalDriverDispatch.toolsTracerExp.pfnSetEnabled, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetCommandListProcAddrTable(
    ze_api_version_t version,
    zet_command_list_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAppendMetricStreamerMarker, L0::globalDriverDispatch.toolsCommandList.pfnAppendMetricStreamerMarker, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMetricQueryBegin, L0::globalDriverDispatch.toolsCommandList.pfnAppendMetricQueryBegin, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMetricQueryEnd, L0::globalDriverDispatch.toolsCommandList.pfnAppendMetricQueryEnd, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAppendMetricMemoryBarrier, L0::globalDriverDispatch.toolsCommandList.pfnAppendMetricMemoryBarrier, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetModuleProcAddrTable(
    ze_api_version_t version,
    zet_module_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetDebugInfo, L0::globalDriverDispatch.toolsModule.pfnGetDebugInfo, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetKernelProcAddrTable(
    ze_api_version_t version,
    zet_kernel_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetProfileInfo, L0::globalDriverDispatch.toolsKernel.pfnGetProfileInfo, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricGroupProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::globalDriverDispatch.toolsMetricGroup.pfnGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.toolsMetricGroup.pfnGetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnCalculateMetricValues, L0::globalDriverDispatch.toolsMetricGroup.pfnCalculateMetricValues, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricProcAddrTable(
    ze_api_version_t version,
    zet_metric_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGet, L0::globalDriverDispatch.toolsMetric.pfnGet, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetProperties, L0::globalDriverDispatch.toolsMetric.pfnGetProperties, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryPoolProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_pool_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.toolsMetricQueryPool.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.toolsMetricQueryPool.pfnDestroy, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricQueryProcAddrTable(
    ze_api_version_t version,
    zet_metric_query_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreate, L0::globalDriverDispatch.toolsMetricQuery.pfnCreate, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDestroy, L0::globalDriverDispatch.toolsMetricQuery.pfnDestroy, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReset, L0::globalDriverDispatch.toolsMetricQuery.pfnReset, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetData, L0::globalDriverDispatch.toolsMetricQuery.pfnGetData, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDeviceProcAddrTable(
    ze_api_version_t version,
    zet_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetDebugProperties, L0::globalDriverDispatch.toolsDevice.pfnGetDebugProperties, version, ZE_API_VERSION_1_0);
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDebugProcAddrTable(
    ze_api_version_t version,
    zet_debug_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAttach, L0::globalDriverDispatch.toolsDebug.pfnAttach, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnDetach, L0::globalDriverDispatch.toolsDebug.pfnDetach, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadEvent, L0::globalDriverDispatch.toolsDebug.pfnReadEvent, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnAcknowledgeEvent, L0::globalDriverDispatch.toolsDebug.pfnAcknowledgeEvent, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnInterrupt, L0::globalDriverDispatch.toolsDebug.pfnInterrupt, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnResume, L0::globalDriverDispatch.toolsDebug.pfnResume, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadMemory, L0::globalDriverDispatch.toolsDebug.pfnReadMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnWriteMemory, L0::globalDriverDispatch.toolsDebug.pfnWriteMemory, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetRegisterSetProperties, L0::globalDriverDispatch.toolsDebug.pfnGetRegisterSetProperties, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnReadRegisters, L0::globalDriverDispatch.toolsDebug.pfnReadRegisters, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnWriteRegisters, L0::globalDriverDispatch.toolsDebug.pfnWriteRegisters, version, ZE_API_VERSION_1_0);
    fillDdiEntry(pDdiTable->pfnGetThreadRegisterSetProperties, L0::globalDriverDispatch.toolsDebug.pfnGetThreadRegisterSetProperties, version, ZE_API_VERSION_1_5);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricGroupExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_group_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCalculateMultipleMetricValuesExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnCalculateMultipleMetricValuesExp, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetGlobalTimestampsExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnGetGlobalTimestampsExp, version, ZE_API_VERSION_1_5);
    fillDdiEntry(pDdiTable->pfnGetExportDataExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnGetExportDataExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnCalculateMetricExportDataExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnCalculateMetricExportDataExp, version, ZE_API_VERSION_1_6);
    fillDdiEntry(pDdiTable->pfnCreateExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnCreateExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnAddMetricExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnAddMetricExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnRemoveMetricExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnRemoveMetricExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnCloseExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnCloseExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::globalDriverDispatch.toolsMetricGroupExp.pfnDestroyExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricProgrammableExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_programmable_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnGetExp, L0::globalDriverDispatch.toolsMetricProgrammableExp.pfnGetExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetPropertiesExp, L0::globalDriverDispatch.toolsMetricProgrammableExp.pfnGetPropertiesExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetParamInfoExp, L0::globalDriverDispatch.toolsMetricProgrammableExp.pfnGetParamInfoExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnGetParamValueInfoExp, L0::globalDriverDispatch.toolsMetricProgrammableExp.pfnGetParamValueInfoExp, version, ZE_API_VERSION_1_9);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnCreateFromProgrammableExp, L0::globalDriverDispatch.toolsMetricExp.pfnCreateFromProgrammableExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::globalDriverDispatch.toolsMetricExp.pfnDestroyExp, version, ZE_API_VERSION_1_9);
    fillDdiEntry(pDdiTable->pfnCreateFromProgrammableExp2, L0::globalDriverDispatch.toolsMetricExp.pfnCreateFromProgrammableExp2, version, ZE_API_VERSION_1_12);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricTracerExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_tracer_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnCreateExp, L0::globalDriverDispatch.toolsMetricTracerExp.pfnCreateExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::globalDriverDispatch.toolsMetricTracerExp.pfnDestroyExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnEnableExp, L0::globalDriverDispatch.toolsMetricTracerExp.pfnEnableExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDisableExp, L0::globalDriverDispatch.toolsMetricTracerExp.pfnDisableExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnReadDataExp, L0::globalDriverDispatch.toolsMetricTracerExp.pfnReadDataExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDecodeExp, L0::globalDriverDispatch.toolsMetricTracerExp.pfnDecodeExp, version, ZE_API_VERSION_1_11);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetMetricDecoderExpProcAddrTable(
    ze_api_version_t version,
    zet_metric_decoder_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnCreateExp, L0::globalDriverDispatch.toolsMetricDecoderExp.pfnCreateExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnDestroyExp, L0::globalDriverDispatch.toolsMetricDecoderExp.pfnDestroyExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnGetDecodableMetricsExp, L0::globalDriverDispatch.toolsMetricDecoderExp.pfnGetDecodableMetricsExp, version, ZE_API_VERSION_1_11);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetDeviceExpProcAddrTable(
    ze_api_version_t version,
    zet_device_exp_dditable_t *pDdiTable) {

    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    ze_result_t result = ZE_RESULT_SUCCESS;

    fillDdiEntry(pDdiTable->pfnGetConcurrentMetricGroupsExp, L0::globalDriverDispatch.toolsDeviceExp.pfnGetConcurrentMetricGroupsExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnCreateMetricGroupsFromMetricsExp, L0::globalDriverDispatch.toolsDeviceExp.pfnCreateMetricGroupsFromMetricsExp, version, ZE_API_VERSION_1_11);
    fillDdiEntry(pDdiTable->pfnEnableMetricsExp, L0::globalDriverDispatch.toolsDeviceExp.pfnEnableMetricsExp, version, ZE_API_VERSION_1_13);
    fillDdiEntry(pDdiTable->pfnDisableMetricsExp, L0::globalDriverDispatch.toolsDeviceExp.pfnDisableMetricsExp, version, ZE_API_VERSION_1_13);

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetGetCommandListExpProcAddrTable(
    ze_api_version_t version,
    zet_command_list_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(L0::globalDriverDispatch.tools.version) != ZE_MAJOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;
    fillDdiEntry(pDdiTable->pfnAppendMarkerExp, L0::globalDriverDispatch.toolsCommandListExp.pfnAppendMarkerExp, version, ZE_API_VERSION_1_13);
    return result;
}
