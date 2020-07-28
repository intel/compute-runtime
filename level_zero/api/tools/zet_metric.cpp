/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric.h"
#include <level_zero/zet_api.h>

#include "third_party/level_zero/zet_api_ext.h"

extern "C" {

__zedllexport ze_result_t __zecall
zetMetricGroupGet(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::metricGroupGet(hDevice, pCount, phMetricGroups);
}

__zedllexport ze_result_t __zecall
zetMetricGroupGetProperties(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_properties_t *pProperties) {

    return L0::MetricGroup::fromHandle(hMetricGroup)->getProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetMetricGroupGetPropertiesExt(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_properties_ext_t *pProperties) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->getPropertiesExt(pProperties);
}

__zedllexport ze_result_t __zecall
zetMetricGet(
    zet_metric_group_handle_t hMetricGroup,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->getMetric(pCount, phMetrics);
}

__zedllexport ze_result_t __zecall
zetMetricGetProperties(
    zet_metric_handle_t hMetric,
    zet_metric_properties_t *pProperties) {
    return L0::Metric::fromHandle(hMetric)->getProperties(pProperties);
}

__zedllexport ze_result_t __zecall
zetMetricGetPropertiesExt(
    zet_metric_handle_t hMetric,
    zet_metric_properties_ext_t *pProperties) {
    return L0::Metric::fromHandle(hMetric)->getPropertiesExt(pProperties);
}

__zedllexport ze_result_t __zecall
zetMetricGroupCalculateMetricValues(
    zet_metric_group_handle_t hMetricGroup,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pMetricValueCount,
    zet_typed_value_t *pMetricValues) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->calculateMetricValues(rawDataSize, pRawData, pMetricValueCount, pMetricValues);
}

__zedllexport ze_result_t __zecall
zetDeviceActivateMetricGroups(
    zet_device_handle_t hDevice,
    uint32_t count,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::Device::fromHandle(hDevice)->activateMetricGroups(count, phMetricGroups);
}

__zedllexport ze_result_t __zecall
zetContextActivateMetricGroups(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t count,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::Context::fromHandle(hContext)->activateMetricGroups(hDevice, count, phMetricGroups);
}

__zedllexport ze_result_t __zecall
zetMetricTracerOpen(
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_tracer_desc_t *pDesc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_tracer_handle_t *phMetricTracer) {
    return L0::metricTracerOpen(hDevice, hMetricGroup, pDesc, hNotificationEvent, phMetricTracer);
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricTracerMarker(
    ze_command_list_handle_t hCommandList,
    zet_metric_tracer_handle_t hMetricTracer,
    uint32_t value) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricTracerMarker(hMetricTracer, value);
}

__zedllexport ze_result_t __zecall
zetMetricTracerClose(
    zet_metric_tracer_handle_t hMetricTracer) {
    return L0::MetricTracer::fromHandle(hMetricTracer)->close();
}

__zedllexport ze_result_t __zecall
zetMetricTracerReadData(
    zet_metric_tracer_handle_t hMetricTracer,
    uint32_t maxReportCount,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::MetricTracer::fromHandle(hMetricTracer)->readData(maxReportCount, pRawDataSize, pRawData);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricStreamerOpen(
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_streamer_desc_t *pDesc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {
    return L0::metricStreamerOpen(hDevice, hMetricGroup, pDesc, hNotificationEvent, phMetricStreamer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetCommandListAppendMetricStreamerMarker(
    ze_command_list_handle_t hCommandList,
    zet_metric_streamer_handle_t hMetricStreamer,
    uint32_t value) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricStreamerMarker(hMetricStreamer, value);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricStreamerClose(
    zet_metric_streamer_handle_t hMetricStreamer) {
    return L0::MetricStreamer::fromHandle(hMetricStreamer)->close();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricStreamerReadData(
    zet_metric_streamer_handle_t hMetricStreamer,
    uint32_t maxReportCount,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::MetricStreamer::fromHandle(hMetricStreamer)->readData(maxReportCount, pRawDataSize, pRawData);
}

__zedllexport ze_result_t __zecall
zetMetricQueryPoolCreate(
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return L0::metricQueryPoolCreate(hDevice, hMetricGroup, desc, phMetricQueryPool);
}

__zedllexport ze_result_t __zecall
zetMetricQueryPoolCreateExt(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    const zet_metric_query_pool_desc_ext_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return L0::metricQueryPoolCreateExt(hContext, hDevice, hMetricGroup, desc, phMetricQueryPool);
}

__zedllexport ze_result_t __zecall
zetMetricQueryPoolDestroy(
    zet_metric_query_pool_handle_t hMetricQueryPool) {
    return L0::MetricQueryPool::fromHandle(hMetricQueryPool)->destroy();
}

__zedllexport ze_result_t __zecall
zetMetricQueryCreate(
    zet_metric_query_pool_handle_t hMetricQueryPool,
    uint32_t index,
    zet_metric_query_handle_t *phMetricQuery) {
    return L0::MetricQueryPool::fromHandle(hMetricQueryPool)->createMetricQuery(index, phMetricQuery);
}

__zedllexport ze_result_t __zecall
zetMetricQueryDestroy(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->destroy();
}

__zedllexport ze_result_t __zecall
zetMetricQueryReset(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->reset();
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricQueryBegin(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryBegin(hMetricQuery);
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricQueryEnd(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery,
    ze_event_handle_t hCompletionEvent) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryEnd(hMetricQuery, hCompletionEvent, 0, nullptr);
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricQueryEndExt(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryEnd(hMetricQuery, hSignalEvent, numWaitEvents, phWaitEvents);
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricMemoryBarrier(
    zet_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricMemoryBarrier();
}

__zedllexport ze_result_t __zecall
zetMetricQueryGetData(
    zet_metric_query_handle_t hMetricQuery,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->getData(pRawDataSize, pRawData);
}

} // extern C
