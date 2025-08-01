/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_programmable_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

namespace L0 {

std::unique_ptr<MetricDeviceContext> MetricDeviceContext::create(Device &device) {
    return std::make_unique<MetricDeviceContext>(device);
}

ze_result_t
metricTracerCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    zet_metric_tracer_exp_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_tracer_exp_handle_t *phMetricTracer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricTracerDestroy(
    zet_metric_tracer_exp_handle_t hMetricTracer) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricTracerEnable(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricTracerDisable(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricTracerReadData(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricDecoderCreate(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    zet_metric_decoder_exp_handle_t *phMetricDecoder) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricDecoderDestroy(
    zet_metric_decoder_exp_handle_t phMetricDecoder) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricDecoderGetDecodableMetrics(
    zet_metric_decoder_exp_handle_t hMetricDecoder,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricTracerDecode(
    zet_metric_decoder_exp_handle_t phMetricDecoder,
    size_t *pRawDataSize,
    uint8_t *pRawData,
    uint32_t metricsCount,
    zet_metric_handle_t *phMetrics,
    uint32_t *pSetCount,
    uint32_t *pMetricEntriesCountPerSet,
    uint32_t *pMetricEntriesCount,
    zet_metric_entry_exp_t *pMetricEntries) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t metricDecodeCalculateMultipleValues(
    zet_intel_metric_decoder_exp_handle_t hMetricDecoder,
    const size_t rawDataSize,
    size_t *offset,
    const uint8_t *pRawData,
    zet_intel_metric_calculation_operation_exp_handle_t hCalculationOperation,
    uint32_t *pSetCount,
    uint32_t *pMetricReportCountPerSet,
    uint32_t *pTotalMetricReportCount,
    zet_intel_metric_result_exp_t *pMetricResults) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0