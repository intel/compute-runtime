/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_tracer_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

ze_result_t MetricTracerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize,
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

    // Read tracer data.
    const ze_result_t result = metricGroup->readIoStream(reportCount, *pRawData);
    if (result == ZE_RESULT_SUCCESS) {
        *pRawDataSize = reportCount * rawReportSize;
    }

    return result;
}

ze_result_t MetricTracerImp::close() {
    const auto result = stopMeasurements();
    if (result == ZE_RESULT_SUCCESS) {

        // Clear metric tracer reference in context.
        auto device = Device::fromHandle(hDevice);
        device->getMetricContext().setMetricTracer(nullptr);

        // Release notification event.
        if (pNotificationEvent != nullptr) {
            pNotificationEvent->metricTracer = nullptr;
        }

        // Delete metric tracer.
        delete this;
    }
    return result;
}

ze_result_t MetricTracerImp::initialize(ze_device_handle_t hDevice,
                                        zet_metric_group_handle_t hMetricGroup) {
    this->hDevice = hDevice;
    this->hMetricGroup = hMetricGroup;

    auto metricGroup = MetricGroup::fromHandle(this->hMetricGroup);
    rawReportSize = metricGroup->getRawReportSize();

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricTracerImp::startMeasurements(uint32_t &notifyEveryNReports,
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

    // Associate notification event with metric tracer.
    pNotificationEvent = Event::fromHandle(hNotificationEvent);
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->metricTracer = this;
    }

    return result;
}

ze_result_t MetricTracerImp::stopMeasurements() {
    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);

    const ze_result_t result = metricGroup->closeIoStream();
    if (result == ZE_RESULT_SUCCESS) {
        oaBufferSize = 0;
    }

    return result;
}

uint32_t MetricTracerImp::getOaBufferSize(const uint32_t notifyEveryNReports) const {
    // Notification is on half full buffer, hence multiplication by 2.
    return notifyEveryNReports * rawReportSize * 2;
}

uint32_t MetricTracerImp::getNotifyEveryNReports(const uint32_t oaBufferSize) const {
    // Notification is on half full buffer, hence division by 2.
    return rawReportSize
               ? oaBufferSize / (rawReportSize * 2)
               : 0;
}

Event::State MetricTracerImp::getNotificationState() {

    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);
    bool reportsReady = metricGroup->waitForReports(0) == ZE_RESULT_SUCCESS;

    return reportsReady
               ? Event::State::STATE_SIGNALED
               : Event::State::STATE_INITIAL;
}

uint32_t MetricTracerImp::getRequiredBufferSize(const uint32_t maxReportCount) const {
    DEBUG_BREAK_IF(rawReportSize == 0);
    uint32_t maxOaBufferReportCount = oaBufferSize / rawReportSize;

    // Trim to OA buffer size if needed.
    return maxReportCount > maxOaBufferReportCount ? oaBufferSize
                                                   : maxReportCount * rawReportSize;
}

MetricTracer *MetricTracer::open(zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                 zet_metric_tracer_desc_t &desc, ze_event_handle_t hNotificationEvent) {
    auto pDevice = Device::fromHandle(hDevice);
    auto &metricContext = pDevice->getMetricContext();

    // Check whether metric tracer is already open.
    if (metricContext.getMetricTracer() != nullptr) {
        return nullptr;
    }

    // Check metric group sampling type.
    auto metricGroupProperties = MetricGroup::getProperties(hMetricGroup);
    if (metricGroupProperties.samplingType != ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED) {
        return nullptr;
    }

    // Check whether metric group is activated.
    if (!metricContext.isMetricGroupActivated(hMetricGroup)) {
        return nullptr;
    }

    auto pMetricTracer = new MetricTracerImp();
    UNRECOVERABLE_IF(pMetricTracer == nullptr);
    pMetricTracer->initialize(hDevice, hMetricGroup);

    const ze_result_t result = pMetricTracer->startMeasurements(
        desc.notifyEveryNReports, desc.samplingPeriod, hNotificationEvent);
    if (result == ZE_RESULT_SUCCESS) {
        metricContext.setMetricTracer(pMetricTracer);
    } else {
        delete pMetricTracer;
        pMetricTracer = nullptr;
    }

    return pMetricTracer;
}

} // namespace L0
