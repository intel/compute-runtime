/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/include/level_zero/zet_intel_gpu_metric.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

ze_result_t ZE_APICALL zetIntelDeviceEnableMetricsExp(zet_device_handle_t hDevice) {
    return L0::metricsEnable(hDevice);
}

ze_result_t ZE_APICALL zetIntelDeviceDisableMetricsExp(zet_device_handle_t hDevice) {
    return L0::metricsDisable(hDevice);
}

ze_result_t ZE_APICALL zetIntelMetricTracerExportExp(zet_metric_tracer_exp_handle_t hMetricTracer,
                                                     zet_intel_metric_export_format_t exportFormat,
                                                     size_t *pSizeOfExportInBytes,
                                                     void *pExportData) {
    return L0::metricTracerExport(hMetricTracer, exportFormat, pSizeOfExportInBytes, pExportData);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationCreateExp(zet_context_handle_t hContext, zet_device_handle_t hDevice,
                                                                   zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
                                                                   zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation) {
    return L0::metricCalculationOperationCreate(hContext, hDevice, pCalculationDesc, phCalculationOperation);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationDestroyExp(zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation) {
    return L0::metricCalculationOperationDestroy(hCalculationOperation);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationGetReportFormatExp(
    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics,
    zet_intel_metric_scope_exp_handle_t *phMetricScopes) {
    return L0::metricCalculationGetReportFormat(hCalculationOperation, pCount, phMetrics, phMetricScopes);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationGetExcludedMetricsExp(
    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::metricCalculationGetExcludedMetrics(hCalculationOperation, pCount, phMetrics);
}

ze_result_t ZE_APICALL zetIntelMetricCalculateValuesExp(const size_t rawDataSize, const uint8_t *pRawData,
                                                        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
                                                        bool lastCall, size_t *usedSize, uint32_t *pTotalMetricReportsCount,
                                                        zet_intel_metric_result_exp_t *pMetricResults) {
    return L0::metricCalculateValues(rawDataSize, pRawData, hCalculationOperation,
                                     lastCall, usedSize,
                                     pTotalMetricReportsCount, pMetricResults);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDecodeCalculateValuesExp(zet_metric_decoder_exp_handle_t hMetricDecoder,
                                                                    const size_t rawDataSize, size_t *usedDataSize, const uint8_t *pRawData,
                                                                    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
                                                                    uint32_t *pTotalMetricReportsCount, zet_intel_metric_result_exp_t *pMetricResults) {
    return L0::metricTracerDecodeCalculateValues(hMetricDecoder, rawDataSize, usedDataSize, pRawData, hCalculationOperation,
                                                 pTotalMetricReportsCount, pMetricResults);
}

ze_result_t ZE_APICALL zetIntelMetricScopesGetExp(zet_context_handle_t hContext, zet_device_handle_t hDevice, uint32_t *pMetricScopesCount, zet_intel_metric_scope_exp_handle_t *phMetricScopes) {
    return L0::metricScopesGet(hContext, hDevice, pMetricScopesCount, phMetricScopes);
}

ze_result_t ZE_APICALL zetIntelMetricScopeGetPropertiesExp(zet_intel_metric_scope_exp_handle_t hMetricScope, zet_intel_metric_scope_properties_exp_t *pMetricScopeProperties) {
    return L0::metricScopeGetProperties(hMetricScope, pMetricScopeProperties);
}

ze_result_t ZE_APICALL
zetIntelMetricTracerDecodeExp2(
    zet_metric_decoder_exp_handle_t phMetricDecoder,
    const size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t metricCount,
    zet_metric_handle_t *phMetric,
    uint32_t metricScopeCount,
    zet_intel_metric_scope_exp_handle_t *phMetricScopes,
    size_t *usedDataSize,
    uint32_t *pTotalMetricEntriesCount,
    zet_metric_entry_exp2_t *phMetricEntries) {
    return L0::metricTracerDecode2(phMetricDecoder, rawDataSize, pRawData, metricCount, phMetric, metricScopeCount, phMetricScopes, usedDataSize, pTotalMetricEntriesCount, phMetricEntries);
}

ze_result_t ZE_APICALL zetIntelMetricSupportedScopesGetExp(zet_metric_handle_t hMetric, uint32_t *pCount, zet_intel_metric_scope_exp_handle_t *phScopes) {
    return L0::getMetricSupportedScopes(hMetric, pCount, phScopes);
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL zetIntelDeviceEnableMetricsExp(zet_device_handle_t hDevice) {
    return L0::zetIntelDeviceEnableMetricsExp(hDevice);
}

ze_result_t ZE_APICALL zetIntelDeviceDisableMetricsExp(zet_device_handle_t hDevice) {
    return L0::zetIntelDeviceDisableMetricsExp(hDevice);
}

ze_result_t ZE_APICALL zetIntelMetricTracerExportExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    zet_intel_metric_export_format_t exportFormat,
    size_t *pSizeOfExportInBytes,
    void *pExportData) {
    return L0::zetIntelMetricTracerExportExp(hMetricTracer, exportFormat, pSizeOfExportInBytes, pExportData);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationCreateExp(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
    zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation) {
    return L0::zetIntelMetricCalculationOperationCreateExp(hContext, hDevice, pCalculationDesc, phCalculationOperation);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationDestroyExp(
    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation) {
    return L0::zetIntelMetricCalculationOperationDestroyExp(hCalculationOperation);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationGetReportFormatExp(
    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics,
    zet_intel_metric_scope_exp_handle_t *phMetricScopes) {
    return L0::zetIntelMetricCalculationOperationGetReportFormatExp(hCalculationOperation, pCount, phMetrics, phMetricScopes);
}

ze_result_t ZE_APICALL zetIntelMetricCalculationOperationGetExcludedMetricsExp(
    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::zetIntelMetricCalculationOperationGetExcludedMetricsExp(hCalculationOperation, pCount, phMetrics);
}

ze_result_t ZE_APICALL zetIntelMetricCalculateValuesExp(const size_t rawDataSize, const uint8_t *pRawData,
                                                        zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
                                                        bool lastCall, size_t *usedSize,
                                                        uint32_t *pTotalMetricReportsCount,
                                                        zet_intel_metric_result_exp_t *pMetricResults) {

    return L0::zetIntelMetricCalculateValuesExp(rawDataSize, pRawData, hCalculationOperation,
                                                lastCall, usedSize,
                                                pTotalMetricReportsCount, pMetricResults);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDecodeCalculateValuesExp(zet_metric_decoder_exp_handle_t hMetricDecoder, const size_t rawDataSize,
                                                                    size_t *usedDataSize, const uint8_t *pRawData, zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
                                                                    uint32_t *pTotalMetricReportsCount, zet_intel_metric_result_exp_t *pMetricResults) {
    return L0::zetIntelMetricTracerDecodeCalculateValuesExp(
        hMetricDecoder, rawDataSize, usedDataSize, pRawData, hCalculationOperation, pTotalMetricReportsCount, pMetricResults);
}

ze_result_t ZE_APICALL zetIntelMetricScopesGetExp(zet_context_handle_t hContext, zet_device_handle_t hDevice, uint32_t *pMetricScopesCount, zet_intel_metric_scope_exp_handle_t *phMetricScopes) {
    return L0::zetIntelMetricScopesGetExp(hContext, hDevice, pMetricScopesCount, phMetricScopes);
}

ze_result_t ZE_APICALL zetIntelMetricScopeGetPropertiesExp(zet_intel_metric_scope_exp_handle_t hMetricScope, zet_intel_metric_scope_properties_exp_t *pMetricScopeProperties) {
    return L0::zetIntelMetricScopeGetPropertiesExp(hMetricScope, pMetricScopeProperties);
}

ze_result_t ZE_APICALL
zetIntelMetricTracerDecodeExp2(
    zet_metric_decoder_exp_handle_t phMetricDecoder,
    const size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t metricCount,
    zet_metric_handle_t *phMetric,
    uint32_t metricScopeCount,
    zet_intel_metric_scope_exp_handle_t *phMetricScopes,
    size_t *usedDataSize,
    uint32_t *pTotalMetricEntriesCount,
    zet_metric_entry_exp2_t *phMetricEntries) {
    return L0::zetIntelMetricTracerDecodeExp2(
        phMetricDecoder,
        rawDataSize,
        pRawData,
        metricCount,
        phMetric,
        metricScopeCount,
        phMetricScopes,
        usedDataSize,
        pTotalMetricEntriesCount,
        phMetricEntries);
}

ze_result_t ZE_APICALL zetIntelMetricSupportedScopesGetExp(zet_metric_handle_t hMetric, uint32_t *pCount, zet_intel_metric_scope_exp_handle_t *phScopes) {
    return L0::getMetricSupportedScopes(hMetric, pCount, phScopes);
}

} // extern "C"
