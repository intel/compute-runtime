/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"
#include <level_zero/zet_api.h>

namespace L0 {

ze_result_t IpSamplingMetricGroupImp::streamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_streamer_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {

    auto device = Device::fromHandle(hDevice);

    // Check whether metric group is activated.
    if (!device->getMetricDeviceContext().isMetricGroupActivated(this->toHandle())) {
        return ZE_RESULT_NOT_READY;
    }

    auto &metricSource = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    // Check whether metric streamer is already open.
    if (metricSource.pActiveStreamer != nullptr) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    auto pStreamerImp = new IpSamplingMetricStreamerImp(metricSource);
    UNRECOVERABLE_IF(pStreamerImp == nullptr);

    const ze_result_t result = metricSource.getMetricOsInterface()->startMeasurement(desc->notifyEveryNReports, desc->samplingPeriod);
    if (result == ZE_RESULT_SUCCESS) {
        metricSource.pActiveStreamer = pStreamerImp;
        pStreamerImp->attachEvent(hNotificationEvent);
    } else {
        delete pStreamerImp;
        pStreamerImp = nullptr;
        return result;
    }

    *phMetricStreamer = pStreamerImp->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) {

    // Return required size if requested.
    if (*pRawDataSize == 0) {
        *pRawDataSize = ipSamplingSource.getMetricOsInterface()->getRequiredBufferSize(maxReportCount);
        return ZE_RESULT_SUCCESS;
    }

    // If there is a difference in pRawDataSize and maxReportCount, use the minimum value for reading.
    if (maxReportCount != UINT32_MAX) {
        size_t maxSizeRequired = ipSamplingSource.getMetricOsInterface()->getRequiredBufferSize(maxReportCount);
        *pRawDataSize = std::min(maxSizeRequired, *pRawDataSize);
    }

    return ipSamplingSource.getMetricOsInterface()->readData(pRawData, pRawDataSize);
}

ze_result_t IpSamplingMetricStreamerImp::close() {

    const ze_result_t result = ipSamplingSource.getMetricOsInterface()->stopMeasurement();
    detachEvent();
    ipSamplingSource.pActiveStreamer = nullptr;
    delete this;

    return result;
}

Event::State IpSamplingMetricStreamerImp::getNotificationState() {

    return ipSamplingSource.getMetricOsInterface()->isNReportsAvailable()
               ? Event::State::STATE_SIGNALED
               : Event::State::STATE_INITIAL;
}

ze_result_t IpSamplingMetricStreamerImp::appendStreamerMarker(CommandList &commandList, uint32_t value) {

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void IpSamplingMetricStreamerImp::attachEvent(ze_event_handle_t hNotificationEvent) {
    // Associate notification event with metric streamer.
    pNotificationEvent = Event::fromHandle(hNotificationEvent);
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->metricStreamer = this;
    }
}

void IpSamplingMetricStreamerImp::detachEvent() {
    // Release notification event.
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->metricStreamer = nullptr;
    }
}

} // namespace L0
