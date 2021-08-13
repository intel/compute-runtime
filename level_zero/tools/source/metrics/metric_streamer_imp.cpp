/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_streamer_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_query_imp.h"

namespace L0 {

ze_result_t MetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize,
                                        uint8_t *pRawData) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    if (metricStreamers.size() > 0) {
        auto pMetricStreamer = MetricStreamer::fromHandle(metricStreamers[0]);
        // Return required size if requested.
        if (*pRawDataSize == 0) {
            *pRawDataSize = static_cast<MetricStreamerImp *>(pMetricStreamer)->getRequiredBufferSize(maxReportCount) * metricStreamers.size();
            return ZE_RESULT_SUCCESS;
        }

        size_t sizePerSubDevice = *pRawDataSize / metricStreamers.size();
        DEBUG_BREAK_IF(sizePerSubDevice == 0);
        *pRawDataSize = 0;
        for (auto metricStreamerHandle : metricStreamers) {

            size_t readSize = sizePerSubDevice;
            pMetricStreamer = MetricStreamer::fromHandle(metricStreamerHandle);
            result = pMetricStreamer->readData(maxReportCount, &readSize, pRawData + *pRawDataSize);
            // Return at first error.
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            *pRawDataSize += readSize;
        }
    } else {

        DEBUG_BREAK_IF(rawReportSize == 0);
        auto metricGroup = MetricGroup::fromHandle(hMetricGroup);

        // Return required size if requested.
        if (*pRawDataSize == 0) {
            *pRawDataSize = getRequiredBufferSize(maxReportCount);
            return ZE_RESULT_SUCCESS;
        }

        // User is expected to allocate space.
        DEBUG_BREAK_IF(pRawData == nullptr);

        // Retrieve the number of reports that fit into the buffer.
        uint32_t reportCount = static_cast<uint32_t>(*pRawDataSize / rawReportSize);

        // Read streamer data.
        result = metricGroup->readIoStream(reportCount, *pRawData);
        if (result == ZE_RESULT_SUCCESS) {
            *pRawDataSize = reportCount * rawReportSize;
        }
    }

    return result;
}

ze_result_t MetricStreamerImp::close() {
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (metricStreamers.size() > 0) {

        for (auto metricStreamerHandle : metricStreamers) {
            auto metricStreamer = MetricStreamer::fromHandle(metricStreamerHandle);
            auto tmpResult = metricStreamer->close();

            // Hold the first error result.
            if (result == ZE_RESULT_SUCCESS)
                result = tmpResult;
        }

        // Delete metric streamer aggregator.
        if (result == ZE_RESULT_SUCCESS) {
            detachEvent();
            delete this;
        }
    } else {

        result = stopMeasurements();
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

            detachEvent();

            // Delete metric streamer.
            delete this;
        }
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
                                                 uint32_t &samplingPeriodNs) {
    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);
    uint32_t requestedOaBufferSize = getOaBufferSize(notifyEveryNReports);

    const ze_result_t result = metricGroup->openIoStream(samplingPeriodNs, requestedOaBufferSize);

    // Return oa buffer size and notification event aligned to gpu capabilities.
    if (result == ZE_RESULT_SUCCESS) {
        oaBufferSize = requestedOaBufferSize;
        notifyEveryNReports = getNotifyEveryNReports(requestedOaBufferSize);
    }

    return result;
}

void MetricStreamerImp::attachEvent(ze_event_handle_t hNotificationEvent) {
    // Associate notification event with metric streamer.
    pNotificationEvent = Event::fromHandle(hNotificationEvent);
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->metricStreamer = this;
    }
}

void MetricStreamerImp::detachEvent() {
    // Release notification event.
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->metricStreamer = nullptr;
    }
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

    if (metricStreamers.size() > 0) {
        for (auto metricStreamer : metricStreamers) {
            // Return Signalled if report is available on any subdevice.
            if (MetricStreamer::fromHandle(metricStreamer)->getNotificationState() == Event::State::STATE_SIGNALED) {
                return Event::State::STATE_SIGNALED;
            }
        }
        return Event::State::STATE_INITIAL;
    }

    auto metricGroup = MetricGroup::fromHandle(hMetricGroup);
    bool reportsReady = metricGroup->waitForReports(0) == ZE_RESULT_SUCCESS;

    return reportsReady
               ? Event::State::STATE_SIGNALED
               : Event::State::STATE_INITIAL;
}

std::vector<zet_metric_streamer_handle_t> &MetricStreamerImp::getMetricStreamers() {
    return metricStreamers;
}

uint32_t MetricStreamerImp::getRequiredBufferSize(const uint32_t maxReportCount) const {
    DEBUG_BREAK_IF(rawReportSize == 0);
    uint32_t maxOaBufferReportCount = oaBufferSize / rawReportSize;

    // Trim to OA buffer size if needed.
    return maxReportCount > maxOaBufferReportCount ? oaBufferSize
                                                   : maxReportCount * rawReportSize;
}

ze_result_t MetricStreamer::openForDevice(Device *pDevice, zet_metric_group_handle_t hMetricGroup,
                                          zet_metric_streamer_desc_t &desc, zet_metric_streamer_handle_t *phMetricStreamer) {
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
    pMetricStreamer->initialize(pDevice->toHandle(), hMetricGroup);

    const ze_result_t result = pMetricStreamer->startMeasurements(
        desc.notifyEveryNReports, desc.samplingPeriod);
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

ze_result_t MetricStreamer::open(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                 zet_metric_streamer_desc_t &desc, ze_event_handle_t hNotificationEvent,
                                 zet_metric_streamer_handle_t *phMetricStreamer) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto pDevice = Device::fromHandle(hDevice);

    if (pDevice->isMultiDeviceCapable()) {
        const auto deviceImp = static_cast<const DeviceImp *>(pDevice);
        const uint32_t subDeviceCount = deviceImp->numSubDevices;
        auto pMetricStreamer = new MetricStreamerImp();
        UNRECOVERABLE_IF(pMetricStreamer == nullptr);

        auto &metricStreamers = pMetricStreamer->getMetricStreamers();
        metricStreamers.resize(subDeviceCount);
        auto metricGroupRootDevice = static_cast<MetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup));

        for (uint32_t i = 0; i < subDeviceCount; i++) {

            auto metricGroupsSubDevice = metricGroupRootDevice->getMetricGroups()[i];
            result = openForDevice(deviceImp->subDevices[i], metricGroupsSubDevice, desc, &metricStreamers[i]);
            if (result != ZE_RESULT_SUCCESS) {
                for (uint32_t j = 0; j < i; j++) {
                    auto metricStreamerSubDevice = MetricStreamer::fromHandle(metricStreamers[j]);
                    delete metricStreamerSubDevice;
                }
                delete pMetricStreamer;
                return result;
            }
        }

        *phMetricStreamer = pMetricStreamer->toHandle();

    } else {
        result = openForDevice(pDevice, hMetricGroup, desc, phMetricStreamer);
    }

    if (result == ZE_RESULT_SUCCESS) {
        MetricStreamerImp *metImp = static_cast<MetricStreamerImp *>(MetricStreamer::fromHandle(*phMetricStreamer));
        metImp->attachEvent(hNotificationEvent);
    }

    return result;
}

} // namespace L0
