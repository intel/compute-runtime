/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric.h"
#include <level_zero/zet_api.h>

namespace L0 {

ze_result_t zetMetricGroupGet(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::metricGroupGet(hDevice, pCount, phMetricGroups);
}

ze_result_t zetMetricGroupGetProperties(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_properties_t *pProperties) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->getProperties(pProperties);
}

ze_result_t zetMetricGet(
    zet_metric_group_handle_t hMetricGroup,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->metricGet(pCount, phMetrics);
}

ze_result_t zetMetricGetProperties(
    zet_metric_handle_t hMetric,
    zet_metric_properties_t *pProperties) {
    return L0::Metric::fromHandle(hMetric)->getProperties(pProperties);
}

ze_result_t zetMetricGroupCalculateMetricValues(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_calculation_type_t type,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pMetricValueCount,
    zet_typed_value_t *pMetricValues) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->calculateMetricValues(type, rawDataSize, pRawData, pMetricValueCount, pMetricValues);
}

ze_result_t zetContextActivateMetricGroups(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t count,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::Context::fromHandle(hContext)->activateMetricGroups(hDevice, count, phMetricGroups);
}

ze_result_t zetMetricStreamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_streamer_desc_t *pDesc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {
    return L0::metricStreamerOpen(hContext, hDevice, hMetricGroup, pDesc, hNotificationEvent, phMetricStreamer);
}

ze_result_t zetCommandListAppendMetricStreamerMarker(
    ze_command_list_handle_t hCommandList,
    zet_metric_streamer_handle_t hMetricStreamer,
    uint32_t value) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricStreamerMarker(hMetricStreamer, value);
}

ze_result_t zetMetricStreamerClose(
    zet_metric_streamer_handle_t hMetricStreamer) {
    return L0::MetricStreamer::fromHandle(hMetricStreamer)->close();
}

ze_result_t zetMetricStreamerReadData(
    zet_metric_streamer_handle_t hMetricStreamer,
    uint32_t maxReportCount,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::MetricStreamer::fromHandle(hMetricStreamer)->readData(maxReportCount, pRawDataSize, pRawData);
}

ze_result_t zetMetricQueryPoolCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return L0::metricQueryPoolCreate(hContext, hDevice, hMetricGroup, desc, phMetricQueryPool);
}

ze_result_t zetMetricQueryPoolDestroy(
    zet_metric_query_pool_handle_t hMetricQueryPool) {
    return L0::MetricQueryPool::fromHandle(hMetricQueryPool)->destroy();
}

ze_result_t zetMetricQueryCreate(
    zet_metric_query_pool_handle_t hMetricQueryPool,
    uint32_t index,
    zet_metric_query_handle_t *phMetricQuery) {
    return L0::MetricQueryPool::fromHandle(hMetricQueryPool)->metricQueryCreate(index, phMetricQuery);
}

ze_result_t zetMetricQueryDestroy(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->destroy();
}

ze_result_t zetMetricQueryReset(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->reset();
}

ze_result_t zetCommandListAppendMetricQueryBegin(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryBegin(hMetricQuery);
}

ze_result_t zetCommandListAppendMetricQueryEnd(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryEnd(hMetricQuery, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zetCommandListAppendMetricMemoryBarrier(
    zet_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->appendMetricMemoryBarrier();
}

ze_result_t zetMetricQueryGetData(
    zet_metric_query_handle_t hMetricQuery,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::MetricQuery::fromHandle(hMetricQuery)->getData(pRawDataSize, pRawData);
}

ze_result_t ZE_APICALL
zetMetricGroupGetExportDataExp(
    zet_metric_group_handle_t hMetricGroup,
    const uint8_t *pRawData,
    size_t rawDataSize,
    size_t *pExportDataSize,
    uint8_t *pExportData) {
    return L0::MetricGroup::fromHandle(hMetricGroup)->getExportData(pRawData, rawDataSize, pExportDataSize, pExportData);
}

ze_result_t ZE_APICALL
zetMetricGroupCalculateMetricExportDataExp(
    ze_driver_handle_t hDriver,
    zet_metric_group_calculation_type_t type,
    size_t exportDataSize,
    const uint8_t *pExportData,
    zet_metric_calculate_exp_desc_t *pCalculationDescriptor,
    uint32_t *pSetCount,
    uint32_t *pTotalMetricValueCount,
    uint32_t *pMetricCounts,
    zet_typed_value_t *pMetricValues) {

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ZE_APICALL
zetMetricProgrammableGetExp(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_metric_programmable_exp_handle_t *phMetricProgrammables) {
    return L0::metricProgrammableGet(hDevice, pCount, phMetricProgrammables);
}

ze_result_t ZE_APICALL
zetMetricProgrammableGetPropertiesExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_exp_properties_t *pProperties) {
    return L0::metricProgrammableGetProperties(hMetricProgrammable, pProperties);
}

ze_result_t ZE_APICALL
zetMetricProgrammableGetParamInfoExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t *pParameterCount,
    zet_metric_programmable_param_info_exp_t *pParameterInfo) {
    return L0::metricProgrammableGetParamInfo(hMetricProgrammable, pParameterCount, pParameterInfo);
}

ze_result_t ZE_APICALL
zetMetricProgrammableGetParamValueInfoExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t parameterOrdinal,
    uint32_t *pValueInfoCount,
    zet_metric_programmable_param_value_info_exp_t *pValueInfo) {
    return L0::metricProgrammableGetParamValueInfo(hMetricProgrammable, parameterOrdinal, pValueInfoCount, pValueInfo);
}

ze_result_t ZE_APICALL
zetMetricCreateFromProgrammableExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_param_value_exp_t *pParameterValues,
    uint32_t parameterCount,
    const char name[ZET_MAX_METRIC_NAME],
    const char description[ZET_MAX_METRIC_DESCRIPTION],
    uint32_t *pMetricHandleCount,
    zet_metric_handle_t *phMetricHandles) {

    return L0::metricCreateFromProgrammable(hMetricProgrammable, pParameterValues, parameterCount, name, description, pMetricHandleCount, phMetricHandles);
}

ze_result_t ZE_APICALL
zetMetricCreateFromProgrammableExp2(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t parameterCount,
    zet_metric_programmable_param_value_exp_t *pParameterValues,
    const char *pName,
    const char *pDescription,
    uint32_t *pMetricHandleCount,
    zet_metric_handle_t *phMetricHandles) {

    return L0::metricCreateFromProgrammable(hMetricProgrammable, pParameterValues, parameterCount, pName, pDescription, pMetricHandleCount, phMetricHandles);
}

ze_result_t ZE_APICALL
zetMetricGroupCreateExp(
    zet_device_handle_t hDevice,
    const char *name,
    const char *description,
    zet_metric_group_sampling_type_flags_t samplingType,
    zet_metric_group_handle_t *pMetricGroupHandle) {

    auto device = Device::fromHandle(hDevice);
    return static_cast<MetricDeviceContext &>(device->getMetricDeviceContext()).metricGroupCreate(name, description, static_cast<zet_metric_group_sampling_type_flag_t>(samplingType), pMetricGroupHandle);
}

ze_result_t ZE_APICALL
zetMetricGroupAddMetricExp(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_handle_t hMetric,
    size_t *errorStringSize,
    char *pErrorString) {
    auto metricGroup = L0::MetricGroup::fromHandle(hMetricGroup);
    return metricGroup->addMetric(hMetric, errorStringSize, pErrorString);
}

ze_result_t ZE_APICALL
zetMetricGroupRemoveMetricExp(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_handle_t hMetric) {
    auto metricGroup = L0::MetricGroup::fromHandle(hMetricGroup);
    return metricGroup->removeMetric(hMetric);
}

ze_result_t ZE_APICALL
zetMetricGroupCloseExp(
    zet_metric_group_handle_t hMetricGroup) {
    auto metricGroup = L0::MetricGroup::fromHandle(hMetricGroup);
    return metricGroup->close();
}
ze_result_t ZE_APICALL
zetMetricGroupDestroyExp(
    zet_metric_group_handle_t hMetricGroup) {
    auto metricGroup = L0::MetricGroup::fromHandle(hMetricGroup);
    return metricGroup->destroy();
}

ze_result_t ZE_APICALL
zetMetricDestroyExp(
    zet_metric_handle_t hMetric) {
    auto metric = L0::Metric::fromHandle(hMetric);
    return metric->destroy();
}

ze_result_t ZE_APICALL
zetDeviceGetConcurrentMetricGroupsExp(
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    uint32_t *pMetricGroupsCountPerConcurrentGroup,
    uint32_t *pConcurrentGroupCount) {

    auto device = Device::fromHandle(toInternalType(hDevice));
    return static_cast<MetricDeviceContext &>(device->getMetricDeviceContext()).getConcurrentMetricGroups(metricGroupCount, phMetricGroups, pConcurrentGroupCount, pMetricGroupsCountPerConcurrentGroup);
}

ze_result_t ZE_APICALL
zetDeviceCreateMetricGroupsFromMetricsExp(
    zet_device_handle_t hDevice,
    uint32_t metricCount,
    zet_metric_handle_t *phMetrics,
    const char *pMetricGroupNamePrefix,
    const char *pDescription,
    uint32_t *pMetricGroupCount,
    zet_metric_group_handle_t *phMetricGroups) {

    hDevice = toInternalType(hDevice);
    auto device = Device::fromHandle(hDevice);
    return device->getMetricDeviceContext().createMetricGroupsFromMetrics(metricCount, phMetrics, pMetricGroupNamePrefix, pDescription, pMetricGroupCount, phMetricGroups);
}

ze_result_t ZE_APICALL
zetMetricTracerCreateExp(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    zet_metric_tracer_exp_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_tracer_exp_handle_t *phMetricTracer) {
    return L0::metricTracerCreate(hContext, hDevice, metricGroupCount, phMetricGroups, desc, hNotificationEvent, phMetricTracer);
}

ze_result_t ZE_APICALL
zetMetricTracerDestroyExp(
    zet_metric_tracer_exp_handle_t hMetricTracer) {
    return L0::metricTracerDestroy(hMetricTracer);
}

ze_result_t ZE_APICALL
zetMetricTracerEnableExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return L0::metricTracerEnable(hMetricTracer, synchronous);
}

ze_result_t ZE_APICALL
zetMetricTracerDisableExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return L0::metricTracerDisable(hMetricTracer, synchronous);
}

ze_result_t ZE_APICALL
zetMetricTracerReadDataExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::metricTracerReadData(hMetricTracer, pRawDataSize, pRawData);
}

ze_result_t ZE_APICALL
zetMetricDecoderCreateExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    zet_metric_decoder_exp_handle_t *phMetricDecoder) {
    return L0::metricDecoderCreate(hMetricTracer, phMetricDecoder);
}

ze_result_t ZE_APICALL
zetMetricDecoderDestroyExp(
    zet_metric_decoder_exp_handle_t phMetricDecoder) {
    return L0::metricDecoderDestroy(phMetricDecoder);
}

ze_result_t ZE_APICALL
zetMetricDecoderGetDecodableMetricsExp(
    zet_metric_decoder_exp_handle_t hMetricDecoder,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::metricDecoderGetDecodableMetrics(
        hMetricDecoder,
        pCount,
        phMetrics);
}

ze_result_t ZE_APICALL
zetMetricTracerDecodeExp(
    zet_metric_decoder_exp_handle_t phMetricDecoder,
    size_t *pRawDataSize,
    uint8_t *pRawData,
    uint32_t metricsCount,
    zet_metric_handle_t *phMetrics,
    uint32_t *pSetCount,
    uint32_t *pMetricEntriesCountPerSet,
    uint32_t *pMetricEntriesCount,
    zet_metric_entry_exp_t *pMetricEntries) {
    return L0::metricTracerDecode(
        phMetricDecoder,
        pRawDataSize,
        pRawData,
        metricsCount,
        phMetrics,
        pSetCount,
        pMetricEntriesCountPerSet,
        pMetricEntriesCount,
        pMetricEntries);
}

ze_result_t ZE_APICALL
zetDeviceEnableMetricsExp(
    zet_device_handle_t hDevice) {
    return L0::metricsEnable(hDevice);
}

ze_result_t ZE_APICALL
zetDeviceDisableMetricsExp(
    zet_device_handle_t hDevice) {
    return L0::metricsDisable(hDevice);
}

ze_result_t ZE_APICALL
zetCommandListAppendMarkerExp(
    zet_command_list_handle_t hCommandList,
    zet_metric_group_handle_t hMetricGroup,
    uint32_t value) {
    return L0::metricAppendMarker(hCommandList, hMetricGroup, value);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricGroupGet(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::zetMetricGroupGet(
        hDevice,
        pCount,
        phMetricGroups);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricGroupGetProperties(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_properties_t *pProperties) {
    return L0::zetMetricGroupGetProperties(
        hMetricGroup,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricGroupCalculateMetricValues(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_calculation_type_t type,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pMetricValueCount,
    zet_typed_value_t *pMetricValues) {
    return L0::zetMetricGroupCalculateMetricValues(
        hMetricGroup,
        type,
        rawDataSize,
        pRawData,
        pMetricValueCount,
        pMetricValues);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricGet(
    zet_metric_group_handle_t hMetricGroup,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::zetMetricGet(
        hMetricGroup,
        pCount,
        phMetrics);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricGetProperties(
    zet_metric_handle_t hMetric,
    zet_metric_properties_t *pProperties) {
    return L0::zetMetricGetProperties(
        hMetric,
        pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetContextActivateMetricGroups(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t count,
    zet_metric_group_handle_t *phMetricGroups) {
    return L0::zetContextActivateMetricGroups(
        hContext,
        hDevice,
        count,
        phMetricGroups);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricStreamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_streamer_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {
    return L0::zetMetricStreamerOpen(
        hContext,
        hDevice,
        hMetricGroup,
        desc,
        hNotificationEvent,
        phMetricStreamer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetCommandListAppendMetricStreamerMarker(
    zet_command_list_handle_t hCommandList,
    zet_metric_streamer_handle_t hMetricStreamer,
    uint32_t value) {
    return L0::zetCommandListAppendMetricStreamerMarker(
        hCommandList,
        hMetricStreamer,
        value);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricStreamerClose(
    zet_metric_streamer_handle_t hMetricStreamer) {
    return L0::zetMetricStreamerClose(
        hMetricStreamer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricStreamerReadData(
    zet_metric_streamer_handle_t hMetricStreamer,
    uint32_t maxReportCount,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::zetMetricStreamerReadData(
        hMetricStreamer,
        maxReportCount,
        pRawDataSize,
        pRawData);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricQueryPoolCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return L0::zetMetricQueryPoolCreate(
        hContext,
        hDevice,
        hMetricGroup,
        desc,
        phMetricQueryPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricQueryPoolDestroy(
    zet_metric_query_pool_handle_t hMetricQueryPool) {
    return L0::zetMetricQueryPoolDestroy(
        hMetricQueryPool);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricQueryCreate(
    zet_metric_query_pool_handle_t hMetricQueryPool,
    uint32_t index,
    zet_metric_query_handle_t *phMetricQuery) {
    return L0::zetMetricQueryCreate(
        hMetricQueryPool,
        index,
        phMetricQuery);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricQueryDestroy(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::zetMetricQueryDestroy(
        hMetricQuery);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricQueryReset(
    zet_metric_query_handle_t hMetricQuery) {
    return L0::zetMetricQueryReset(
        hMetricQuery);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetCommandListAppendMetricQueryBegin(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery) {
    return L0::zetCommandListAppendMetricQueryBegin(
        hCommandList,
        hMetricQuery);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetCommandListAppendMetricQueryEnd(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zetCommandListAppendMetricQueryEnd(
        hCommandList,
        hMetricQuery,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetCommandListAppendMetricMemoryBarrier(
    zet_command_list_handle_t hCommandList) {
    return L0::zetCommandListAppendMetricMemoryBarrier(
        hCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zetMetricQueryGetData(
    zet_metric_query_handle_t hMetricQuery,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::zetMetricQueryGetData(
        hMetricQuery,
        pRawDataSize,
        pRawData);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupGetExportDataExp(
    zet_metric_group_handle_t hMetricGroup,
    const uint8_t *pRawData,
    size_t rawDataSize,
    size_t *pExportDataSize,
    uint8_t *pExportData) {
    return L0::zetMetricGroupGetExportDataExp(hMetricGroup, pRawData, rawDataSize, pExportDataSize, pExportData);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupCalculateMetricExportDataExp(
    ze_driver_handle_t hDriver,
    zet_metric_group_calculation_type_t type,
    size_t exportDataSize,
    const uint8_t *pExportData,
    zet_metric_calculate_exp_desc_t *pCalculationDescriptor,
    uint32_t *pSetCount,
    uint32_t *pTotalMetricValueCount,
    uint32_t *pMetricCounts,
    zet_typed_value_t *pMetricValues) {
    return L0::zetMetricGroupCalculateMetricExportDataExp(hDriver, type, exportDataSize, pExportData, pCalculationDescriptor,
                                                          pSetCount, pTotalMetricValueCount, pMetricCounts, pMetricValues);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricProgrammableGetExp(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_metric_programmable_exp_handle_t *phMetricProgrammables) {
    return L0::zetMetricProgrammableGetExp(hDevice, pCount, phMetricProgrammables);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricProgrammableGetPropertiesExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_exp_properties_t *pProperties) {
    return L0::zetMetricProgrammableGetPropertiesExp(hMetricProgrammable, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricProgrammableGetParamInfoExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t *pParameterCount,
    zet_metric_programmable_param_info_exp_t *pParameterInfo) {
    return L0::zetMetricProgrammableGetParamInfoExp(hMetricProgrammable, pParameterCount, pParameterInfo);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricProgrammableGetParamValueInfoExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t parameterOrdinal,
    uint32_t *pValueInfoCount,
    zet_metric_programmable_param_value_info_exp_t *pValueInfo) {
    return L0::zetMetricProgrammableGetParamValueInfoExp(hMetricProgrammable, parameterOrdinal, pValueInfoCount, pValueInfo);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricCreateFromProgrammableExp(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_param_value_exp_t *pParameterValues,
    uint32_t parameterCount,
    const char name[ZET_MAX_METRIC_NAME],
    const char description[ZET_MAX_METRIC_DESCRIPTION],
    uint32_t *pMetricHandleCount,
    zet_metric_handle_t *phMetricHandles) {
    return L0::zetMetricCreateFromProgrammableExp(hMetricProgrammable, pParameterValues, parameterCount, name, description, pMetricHandleCount, phMetricHandles);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricCreateFromProgrammableExp2(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t parameterCount,
    zet_metric_programmable_param_value_exp_t *pParameterValues,
    const char *pName,
    const char *pDescription,
    uint32_t *pMetricHandleCount,
    zet_metric_handle_t *phMetricHandles) {
    return L0::zetMetricCreateFromProgrammableExp2(hMetricProgrammable, parameterCount, pParameterValues, pName, pDescription, pMetricHandleCount, phMetricHandles);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupCreateExp(
    zet_device_handle_t hDevice,
    const char *name,
    const char *description,
    zet_metric_group_sampling_type_flags_t samplingType,
    zet_metric_group_handle_t *pMetricGroupHandle) {
    return L0::zetMetricGroupCreateExp(hDevice, name, description, samplingType, pMetricGroupHandle);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupAddMetricExp(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_handle_t hMetric,
    size_t *errorStringSize,
    char *pErrorString) {
    return L0::zetMetricGroupAddMetricExp(hMetricGroup, hMetric, errorStringSize, pErrorString);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupRemoveMetricExp(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_handle_t hMetric) {
    return L0::zetMetricGroupRemoveMetricExp(hMetricGroup, hMetric);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupCloseExp(
    zet_metric_group_handle_t hMetricGroup) {
    return L0::zetMetricGroupCloseExp(hMetricGroup);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupDestroyExp(
    zet_metric_group_handle_t hMetricGroup) {
    return L0::zetMetricGroupDestroyExp(hMetricGroup);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricDestroyExp(
    zet_metric_handle_t hMetric) {
    return L0::zetMetricDestroyExp(hMetric);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDeviceGetConcurrentMetricGroupsExp(
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    uint32_t *pMetricGroupsCountPerConcurrentGroup,
    uint32_t *pConcurrentGroupCount) {
    return L0::zetDeviceGetConcurrentMetricGroupsExp(
        hDevice,
        metricGroupCount,
        phMetricGroups,
        pMetricGroupsCountPerConcurrentGroup,
        pConcurrentGroupCount);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDeviceCreateMetricGroupsFromMetricsExp(
    zet_device_handle_t hDevice,
    uint32_t metricCount,
    zet_metric_handle_t *phMetrics,
    const char *pMetricGroupNamePrefix,
    const char *pDescription,
    uint32_t *pMetricGroupCount,
    zet_metric_group_handle_t *phMetricGroup) {
    return L0::zetDeviceCreateMetricGroupsFromMetricsExp(
        hDevice,
        metricCount,
        phMetrics,
        pMetricGroupNamePrefix,
        pDescription,
        pMetricGroupCount,
        phMetricGroup);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerCreateExp(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    zet_metric_tracer_exp_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_tracer_exp_handle_t *phMetricTracer) {

    return L0::zetMetricTracerCreateExp(
        hContext,
        hDevice,
        metricGroupCount,
        phMetricGroups,
        desc,
        hNotificationEvent,
        phMetricTracer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerDestroyExp(
    zet_metric_tracer_exp_handle_t hMetricTracer) {
    return L0::zetMetricTracerDestroyExp(
        hMetricTracer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerEnableExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return L0::zetMetricTracerEnableExp(
        hMetricTracer,
        synchronous);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerDisableExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return L0::zetMetricTracerDisableExp(
        hMetricTracer,
        synchronous);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerReadDataExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::zetMetricTracerReadDataExp(
        hMetricTracer,
        pRawDataSize,
        pRawData);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricDecoderCreateExp(
    zet_metric_tracer_exp_handle_t hMetricTracer,
    zet_metric_decoder_exp_handle_t *phMetricDecoder) {

    return L0::zetMetricDecoderCreateExp(
        hMetricTracer,
        phMetricDecoder);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricDecoderDestroyExp(
    zet_metric_decoder_exp_handle_t phMetricDecoder) {

    return L0::zetMetricDecoderDestroyExp(
        phMetricDecoder);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricDecoderGetDecodableMetricsExp(
    zet_metric_decoder_exp_handle_t hMetricDecoder,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {

    return L0::zetMetricDecoderGetDecodableMetricsExp(
        hMetricDecoder,
        pCount,
        phMetrics);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricTracerDecodeExp(
    zet_metric_decoder_exp_handle_t phMetricDecoder,
    size_t *pRawDataSize,
    uint8_t *pRawData,
    uint32_t metricsCount,
    zet_metric_handle_t *phMetrics,
    uint32_t *pSetCount,
    uint32_t *pMetricEntriesCountPerSet,
    uint32_t *pMetricEntriesCount,
    zet_metric_entry_exp_t *pMetricEntries) {
    return L0::zetMetricTracerDecodeExp(
        phMetricDecoder,
        pRawDataSize,
        pRawData,
        metricsCount,
        phMetrics,
        pSetCount,
        pMetricEntriesCountPerSet,
        pMetricEntriesCount,
        pMetricEntries);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDeviceEnableMetricsExp(
    zet_device_handle_t hDevice) {
    return L0::zetDeviceEnableMetricsExp(hDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetDeviceDisableMetricsExp(
    zet_device_handle_t hDevice) {
    return L0::zetDeviceDisableMetricsExp(hDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetCommandListAppendMarkerExp(
    zet_command_list_handle_t hCommandList,
    zet_metric_group_handle_t hMetricGroup,
    uint32_t value) {
    return L0::zetCommandListAppendMarkerExp(hCommandList, hMetricGroup, value);
}
} // extern "C"
