/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_streamer_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_query_imp.h"

namespace L0 {

ze_result_t MetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize,
                                        uint8_t *pRawData) {
    DEBUG_BREAK_IF(rawReportSize == 0);

    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);

    // Return required size if requested.
    if (*pRawDataSize == 0) {
        *pRawDataSize = getRequiredBufferSize(maxReportCount);
        return ZE_RESULT_SUCCESS;
    }

    // User is expected to allocate space.
    if (pRawData == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Retrieve the number of reports that fit into the buffer.
    uint32_t reportCount = static_cast<uint32_t>(*pRawDataSize / rawReportSize);

    // Read streamer data.
    const ze_result_t result = metricGroup->readIoStream(reportCount, *pRawData);
    if (result == ZE_RESULT_SUCCESS) {
        *pRawDataSize = reportCount * rawReportSize;
    }

    return result;
}

ze_result_t MetricStreamerImp::close() {
    const auto result = stopMeasurements();
    if (result == ZE_RESULT_SUCCESS) {

        auto device = Device::fromHandle(hDevice);
        auto &metricContext = device->getMetricContext();
        auto &metricsLibrary = metricContext.getMetricsLibrary();

        // Clear metric streamer reference in context.
        // Another metric streamer instance or query can be used.
        metricContext.setMetricStreamer(nullptr);

        // Close metrics library (if was used to generate streamer's marker gpu commands).
        // It will allow metric query to use Linux Tbs stream exclusively
        // (to activate metric sets and to read context switch reports).
        metricsLibrary.release();

        // Release notification event.
        if (pNotificationEvent != nullptr) {
            pNotificationEvent->metricStreamer = nullptr;
        }

        // Delete metric streamer.
        delete this;
    }
    return result;
}

ze_result_t MetricStreamerImp::initialize(ze_device_handle_t hDevice,
                                          zet_metric_group_handle_t hMetricGroup) {
    this->hDevice = hDevice;
    this->hMetricGroup = hMetricGroup;

    auto metricGroup = MetricGroup::fromHandle(this->hMetricGroup);
    rawReportSize = metricGroup->getRawReportSize();

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricStreamerImp::startMeasurements(uint32_t &notifyEveryNReports,
                                                 uint32_t &samplingPeriodNs,
                                                 ze_event_handle_t hNotificationEvent) {
    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);
    uint32_t requestedOaBufferSize = getOaBufferSize(notifyEveryNReports);

    const ze_result_t result = metricGroup->openIoStream(samplingPeriodNs, requestedOaBufferSize);

    // Return oa buffer size and notification event aligned to gpu capabilities.
    if (result == ZE_RESULT_SUCCESS) {
        oaBufferSize = requestedOaBufferSize;
        notifyEveryNReports = getNotifyEveryNReports(requestedOaBufferSize);
    }

    // Associate notification event with metric streamer.
    pNotificationEvent = Event::fromHandle(hNotificationEvent);
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->metricStreamer = this;
    }

    return result;
}

ze_result_t MetricStreamerImp::stopMeasurements() {
    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);

    const ze_result_t result = metricGroup->closeIoStream();
    if (result == ZE_RESULT_SUCCESS) {
        oaBufferSize = 0;
    }

    return result;
}

uint32_t MetricStreamerImp::getOaBufferSize(const uint32_t notifyEveryNReports) const {
    // Notification is on half full buffer, hence multiplication by 2.
    return notifyEveryNReports * rawReportSize * 2;
}

uint32_t MetricStreamerImp::getNotifyEveryNReports(const uint32_t oaBufferSize) const {
    // Notification is on half full buffer, hence division by 2.
    return rawReportSize
               ? oaBufferSize / (rawReportSize * 2)
               : 0;
}

Event::State MetricStreamerImp::getNotificationState() {

    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);
    bool reportsReady = metricGroup->waitForReports(0) == ZE_RESULT_SUCCESS;

    return reportsReady
               ? Event::State::STATE_SIGNALED
               : Event::State::STATE_INITIAL;
}

uint32_t MetricStreamerImp::getRequiredBufferSize(const uint32_t maxReportCount) const {
    DEBUG_BREAK_IF(rawReportSize == 0);
    uint32_t maxOaBufferReportCount = oaBufferSize / rawReportSize;

    // Trim to OA buffer size if needed.
    return maxReportCount > maxOaBufferReportCount ? oaBufferSize
                                                   : maxReportCount * rawReportSize;
}

ze_result_t MetricStreamer::open(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                 zet_metric_streamer_desc_t &desc, ze_event_handle_t hNotificationEvent,
                                 zet_metric_streamer_handle_t *phMetricStreamer) {
    auto pDevice = Device::fromHandle(hDevice);
    auto &metricContext = pDevice->getMetricContext();

    *phMetricStreamer = nullptr;

    // Check whether metric streamer is already open.
    if (metricContext.getMetricStreamer() != nullptr) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    // metric streamer cannot be used with query simultaneously
    // (oa buffer cannot be shared).
    if (metricContext.getMetricsLibrary().getMetricQueryCount() > 0) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Unload metrics library if there are no active queries.
    // It will allow to open metric streamer. Query and streamer cannot be used
    // simultaneously since they use the same exclusive resource (oa buffer).
    if (metricContext.getMetricsLibrary().getInitializationState() == ZE_RESULT_SUCCESS) {
        metricContext.getMetricsLibrary().release();
    }

    // Check metric group sampling type.
    auto metricGroupProperties = MetricGroup::getProperties(hMetricGroup);
    if (metricGroupProperties.samplingType != ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Check whether metric group is activated.
    if (!metricContext.isMetricGroupActivated(hMetricGroup)) {
        return ZE_RESULT_NOT_READY;
    }

    auto pMetricStreamer = new MetricStreamerImp();
    UNRECOVERABLE_IF(pMetricStreamer == nullptr);
    pMetricStreamer->initialize(hDevice, hMetricGroup);

    const ze_result_t result = pMetricStreamer->startMeasurements(
        desc.notifyEveryNReports, desc.samplingPeriod, hNotificationEvent);
    if (result == ZE_RESULT_SUCCESS) {
        metricContext.setMetricStreamer(pMetricStreamer);
    } else {
        delete pMetricStreamer;
        pMetricStreamer = nullptr;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    *phMetricStreamer = pMetricStreamer->toHandle();
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
