/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric.h"
#include <level_zero/zet_api.h>

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricGroupGet(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::metricGroupGet(hDevice, pCount, phMetricGroups);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricGroupGetProperties(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_properties_t *pProperties) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->getProperties(pProperties);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricGet(
    zet_metric_group_handle_t hMetricGroup,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->metricGet(pCount, phMetrics);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricGetProperties(
    zet_metric_handle_t hMetric,
    zet_metric_properties_t *pProperties) {
    return L0::Metric::fromHandle(hMetric)->getProperties(pProperties);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricGroupCalculateMetricValues(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_calculation_type_t type,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pMetricValueCount,
    zet_typed_value_t *pMetricValues) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->calculateMetricValues(type, rawDataSize, pRawData, pMetricValueCount, pMetricValues);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetContextActivateMetricGroups(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t count,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::Context::fromHandle(hContext)->activateMetricGroups(hDevice, count, phMetricGroups);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricStreamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_streamer_desc_t *pDesc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {
    return L0::metricStreamerOpen(hContext, hDevice, hMetricGroup, pDesc, hNotificationEvent, phMetricStreamer);
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

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricQueryPoolCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return L0::metricQueryPoolCreate(hContext, hDevice, hMetricGroup, desc, phMetricQueryPool);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricQueryPoolDestroy(
    zet_metric_query_pool_handle_t hMetricQueryPool) {
    return L0::MetricQueryPool::fromHandle(hMetricQueryPool)->destroy();
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricQueryCreate(
    zet_metric_query_pool_handle_t hMetricQueryPool,
    uint32_t index,
    zet_metric_query_handle_t *phMetricQuery) {
    return L0::MetricQueryPool::fromHandle(hMetricQueryPool)->metricQueryCreate(index, phMetricQuery);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricQueryDestroy(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->destroy();
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricQueryReset(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->reset();
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetCommandListAppendMetricQueryBegin(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryBegin(hMetricQuery);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetCommandListAppendMetricQueryEnd(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryEnd(hMetricQuery, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetCommandListAppendMetricMemoryBarrier(
    zet_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricMemoryBarrier();
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zetMetricQueryGetData(
    zet_metric_query_handle_t hMetricQuery,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->getData(pRawDataSize, pRawData);
}
