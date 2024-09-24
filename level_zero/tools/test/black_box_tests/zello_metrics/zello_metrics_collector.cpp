/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <cstring>
#include <thread>
#include <vector>

namespace zmu = ZelloMetricsUtility;

///////////////////////////
/////SingleMetricCollector
///////////////////////////
SingleMetricCollector::SingleMetricCollector(ExecutionContext *executionCtxt,
                                             const char *metricGroupName,
                                             const zet_metric_group_sampling_type_flag_t samplingType)
    : Collector(executionCtxt), samplingType(samplingType) {

    metricGroup = zmu::findMetricGroup(metricGroupName, samplingType, executionCtxt->getDeviceHandle(0));
    zmu::printMetricGroupAndMetricProperties(metricGroup);
}

SingleMetricCollector::SingleMetricCollector(ExecutionContext *executionCtxt,
                                             zet_metric_group_handle_t metricGroup,
                                             const zet_metric_group_sampling_type_flag_t samplingType)
    : Collector(executionCtxt), metricGroup(metricGroup), samplingType(samplingType) {

    zmu::printMetricGroupAndMetricProperties(metricGroup);
}

///////////////////////////////////
/////SingleMetricStreamerCollector
///////////////////////////////////
SingleMetricStreamerCollector::SingleMetricStreamerCollector(ExecutionContext *executionCtxt,
                                                             const char *metricGroupName) : SingleMetricCollector(executionCtxt, metricGroupName,
                                                                                                                  ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {

    executionCtxt->addActiveMetricGroup(metricGroup);
}

SingleMetricStreamerCollector::SingleMetricStreamerCollector(ExecutionContext *executionCtxt,
                                                             zet_metric_group_handle_t metricGroup) : SingleMetricCollector(executionCtxt, metricGroup,
                                                                                                                            ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {

    executionCtxt->addActiveMetricGroup(metricGroup);
}

bool SingleMetricStreamerCollector::start() {

    eventPool = zmu::createHostVisibleEventPool(executionCtxt->getContextHandle(0), executionCtxt->getDeviceHandle(0));
    notificationEvent = zmu::createHostVisibleEvent(eventPool);

    zet_metric_streamer_desc_t streamerProperties = {};
    streamerProperties.notifyEveryNReports = notifyReportCount;
    streamerProperties.samplingPeriod = samplingPeriod;
    VALIDATECALL(zetMetricStreamerOpen(executionCtxt->getContextHandle(0),
                                       executionCtxt->getDeviceHandle(0),
                                       metricGroup,
                                       &streamerProperties,
                                       notificationEvent,
                                       &metricStreamer));
    // Initial pause
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return true;
}

bool SingleMetricStreamerCollector::stop() {

    VALIDATECALL(zetMetricStreamerClose(metricStreamer));
    VALIDATECALL(zeEventDestroy(notificationEvent));
    VALIDATECALL(zeEventPoolDestroy(eventPool));
    return true;
}

bool SingleMetricStreamerCollector::prefixCommands() {
    return true;
}

bool SingleMetricStreamerCollector::suffixCommands() {
    return true;
}

bool SingleMetricStreamerCollector::isDataAvailable() {

    auto eventStatus = zeEventQueryStatus(notificationEvent);
    zeEventHostReset(notificationEvent);
    return eventStatus == ZE_RESULT_SUCCESS;
}

void SingleMetricStreamerCollector::showResults() {

    // Read raw buffer size. x2 to request more than the available reports
    uint32_t maxRawReportCount = std::max(maxRequestRawReportCount, notifyReportCount * 2);
    size_t rawDataSize = 0;
    std::vector<uint8_t> rawData{};

    VALIDATECALL(zetMetricStreamerReadData(metricStreamer, maxRawReportCount, &rawDataSize, nullptr));
    LOG(zmu::LogLevel::DEBUG) << "Streamer read requires: " << rawDataSize << " bytes buffer" << std::endl;

    // Read raw data.
    rawData.resize(rawDataSize, 0);
    ze_result_t status = zetMetricStreamerReadData(metricStreamer, maxRawReportCount, &rawDataSize, rawData.data());
    LOG(zmu::LogLevel::DEBUG) << "Streamer read raw bytes: " << rawDataSize << std::endl;

    if (status == ZE_RESULT_WARNING_DROPPED_DATA) {
        rawDataSize = (uint32_t)rawData.size();
        zmu::sleep(5);
        VALIDATECALL(zetMetricStreamerReadData(metricStreamer, maxRawReportCount, &rawDataSize, rawData.data()));
        LOG(zmu::LogLevel::DEBUG) << "Streamer read raw bytes: " << rawDataSize << std::endl;
    }
    zmu::obtainCalculatedMetrics(metricGroup, rawData.data(), static_cast<uint32_t>(rawDataSize));
}

////////////////////////////////
/////SingleMetricQueryCollector
////////////////////////////////
SingleMetricQueryCollector::SingleMetricQueryCollector(ExecutionContext *executionCtxt,
                                                       const char *metricGroupName) : SingleMetricCollector(executionCtxt, metricGroupName,
                                                                                                            ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED) {

    executionCtxt->addActiveMetricGroup(metricGroup);
}

SingleMetricQueryCollector::SingleMetricQueryCollector(ExecutionContext *executionCtxt,
                                                       zet_metric_group_handle_t metricGroup) : SingleMetricCollector(executionCtxt, metricGroup,
                                                                                                                      ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED) {
    executionCtxt->addActiveMetricGroup(metricGroup);
}

bool SingleMetricQueryCollector::start() {

    eventPool = zmu::createHostVisibleEventPool(executionCtxt->getContextHandle(0), executionCtxt->getDeviceHandle(0));
    notificationEvent = zmu::createHostVisibleEvent(eventPool);
    queryPoolDesc.count = queryPoolCount;
    queryPoolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

    // Create metric query pool.
    VALIDATECALL(zetMetricQueryPoolCreate(executionCtxt->getContextHandle(0), executionCtxt->getDeviceHandle(0), metricGroup, &queryPoolDesc, &queryPoolHandle));

    // Obtain metric query from pool.
    VALIDATECALL(zetMetricQueryCreate(queryPoolHandle, querySlotIndex, &queryHandle));
    return true;
}

bool SingleMetricQueryCollector::stop() {

    VALIDATECALL(zetMetricQueryDestroy(queryHandle));
    VALIDATECALL(zetMetricQueryPoolDestroy(queryPoolHandle));

    VALIDATECALL(zeEventDestroy(notificationEvent));
    VALIDATECALL(zeEventPoolDestroy(eventPool));
    return true;
}

bool SingleMetricQueryCollector::prefixCommands() {
    VALIDATECALL(zetCommandListAppendMetricQueryBegin(executionCtxt->getCommandList(0), queryHandle));
    return true;
}

bool SingleMetricQueryCollector::suffixCommands() {
    // Metric query end.
    VALIDATECALL(zetCommandListAppendMetricQueryEnd(executionCtxt->getCommandList(0), queryHandle, notificationEvent, 0, nullptr));

    // An optional memory barrier to flush gpu caches.
    VALIDATECALL(zetCommandListAppendMetricMemoryBarrier(executionCtxt->getCommandList(0)));
    return true;
}

bool SingleMetricQueryCollector::isDataAvailable() {
    return zeEventQueryStatus(notificationEvent) == ZE_RESULT_SUCCESS;
}

void SingleMetricQueryCollector::setAttributes(uint32_t notifyReportCount, zet_metric_query_pool_type_t queryPoolType) {
    this->notifyReportCount = notifyReportCount;
    this->queryPoolType = queryPoolType;
}

void SingleMetricQueryCollector::showResults() {

    size_t rawDataSize = 0;
    std::vector<uint8_t> rawData{};
    // Obtain metric query report size.
    VALIDATECALL(zetMetricQueryGetData(queryHandle, &rawDataSize, nullptr));

    // Obtain report.
    rawData.resize(rawDataSize, 0);
    VALIDATECALL(zetMetricQueryGetData(queryHandle, &rawDataSize, rawData.data()));

    zmu::obtainCalculatedMetrics(metricGroup, rawData.data(), static_cast<uint32_t>(rawDataSize));
}
