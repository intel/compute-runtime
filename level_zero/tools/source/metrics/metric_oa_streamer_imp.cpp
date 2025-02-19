/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_streamer_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

using namespace MetricsLibraryApi;

namespace L0 {

ze_result_t OaMetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize,
                                          uint8_t *pRawData) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    const size_t metricStreamerSize = metricStreamers.size();

    if (metricStreamerSize > 0) {
        auto pMetricStreamer = MetricStreamer::fromHandle(metricStreamers[0]);
        // Return required size if requested.
        if (*pRawDataSize == 0) {
            const size_t headerSize = sizeof(MetricGroupCalculateHeader);
            const size_t rawDataOffsetsRequiredSize = sizeof(uint32_t) * metricStreamerSize;
            const size_t rawDataSizesRequiredSize = sizeof(uint32_t) * metricStreamerSize;
            const size_t rawDataRequiredSize = static_cast<OaMetricStreamerImp *>(pMetricStreamer)->getRequiredBufferSize(maxReportCount) * metricStreamerSize;
            *pRawDataSize = headerSize + rawDataOffsetsRequiredSize + rawDataSizesRequiredSize + rawDataRequiredSize;
            return ZE_RESULT_SUCCESS;
        }

        MetricGroupCalculateHeader *pRawDataHeader = reinterpret_cast<MetricGroupCalculateHeader *>(pRawData);
        pRawDataHeader->magic = MetricGroupCalculateHeader::magicValue;
        pRawDataHeader->dataCount = static_cast<uint32_t>(metricStreamerSize);

        // Relative offsets in the header allow to move/copy the buffer.
        pRawDataHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
        pRawDataHeader->rawDataSizes = static_cast<uint32_t>(pRawDataHeader->rawDataOffsets + (sizeof(uint32_t) * metricStreamerSize));
        pRawDataHeader->rawDataOffset = static_cast<uint32_t>(pRawDataHeader->rawDataSizes + (sizeof(uint32_t) * metricStreamerSize));

        const size_t sizePerSubDevice = (*pRawDataSize - pRawDataHeader->rawDataOffset) / metricStreamerSize;
        DEBUG_BREAK_IF(sizePerSubDevice == 0);
        *pRawDataSize = pRawDataHeader->rawDataOffset;

        uint32_t *pRawDataOffsetsUnpacked = reinterpret_cast<uint32_t *>(pRawData + pRawDataHeader->rawDataOffsets);
        uint32_t *pRawDataSizesUnpacked = reinterpret_cast<uint32_t *>(pRawData + pRawDataHeader->rawDataSizes);
        uint8_t *pRawDataUnpacked = reinterpret_cast<uint8_t *>(pRawData + pRawDataHeader->rawDataOffset);

        for (size_t i = 0; i < metricStreamerSize; ++i) {

            size_t readSize = sizePerSubDevice;
            const uint32_t rawDataOffset = (i != 0) ? (pRawDataSizesUnpacked[i - 1] + pRawDataOffsetsUnpacked[i - 1]) : 0;
            pMetricStreamer = MetricStreamer::fromHandle(metricStreamers[i]);
            result = pMetricStreamer->readData(maxReportCount, &readSize, pRawDataUnpacked + rawDataOffset);
            // Return at first error.
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            pRawDataSizesUnpacked[i] = static_cast<uint32_t>(readSize);
            pRawDataOffsetsUnpacked[i] = (i != 0) ? pRawDataOffsetsUnpacked[i - 1] + pRawDataSizesUnpacked[i - 1] : 0;
            *pRawDataSize += readSize;
        }
    } else {

        DEBUG_BREAK_IF(rawReportSize == 0);
        auto metricGroup = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup));

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
        if (result == ZE_RESULT_SUCCESS || result == ZE_RESULT_WARNING_DROPPED_DATA) {
            *pRawDataSize = reportCount * rawReportSize;
        } else {
            *pRawDataSize = 0;
        }
    }

    return result;
}

ze_result_t OaMetricStreamerImp::close() {
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
            auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
            auto &metricsLibrary = metricSource.getMetricsLibrary();

            // Clear metric streamer reference in context.
            // Another metric streamer instance or query can be used.
            metricSource.setMetricStreamer(nullptr);

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

ze_result_t OaMetricStreamerImp::initialize(ze_device_handle_t hDevice,
                                            zet_metric_group_handle_t hMetricGroup) {
    this->hDevice = hDevice;
    this->hMetricGroup = hMetricGroup;

    auto metricGroup = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(this->hMetricGroup));
    rawReportSize = metricGroup->getRawReportSize();

    return ZE_RESULT_SUCCESS;
}

uint32_t OaMetricStreamerImp::getOaBufferSize(const uint32_t notifyEveryNReports) const {

    // Notification is on half full buffer, hence multiplication by 2.
    return notifyEveryNReports * rawReportSize * 2;
}

ze_result_t OaMetricStreamerImp::startMeasurements(uint32_t &notifyEveryNReports,
                                                   uint32_t &samplingPeriodNs) {
    auto metricGroup = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup));
    uint32_t requestedOaBufferSize = getOaBufferSize(notifyEveryNReports);

    const ze_result_t result = metricGroup->openIoStream(samplingPeriodNs, requestedOaBufferSize);

    // Return oa buffer size and notification event aligned to gpu capabilities.
    if (result == ZE_RESULT_SUCCESS) {
        oaBufferSize = requestedOaBufferSize;
        notifyEveryNReports = getNotifyEveryNReports(requestedOaBufferSize);
    }

    return result;
}

ze_result_t OaMetricStreamerImp::stopMeasurements() {
    auto metricGroup = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup));

    const ze_result_t result = metricGroup->closeIoStream();
    if (result == ZE_RESULT_SUCCESS) {
        oaBufferSize = 0;
    }

    return result;
}

uint32_t OaMetricStreamerImp::getNotifyEveryNReports(const uint32_t oaBufferSize) const {
    // Notification is on half full buffer, hence division by 2.
    return rawReportSize
               ? oaBufferSize / (rawReportSize * 2)
               : 0;
}

Event::State OaMetricStreamerImp::getNotificationState() {

    if (metricStreamers.size() > 0) {
        for (auto metricStreamer : metricStreamers) {
            // Return Signalled if report is available on any subdevice.
            if (MetricStreamer::fromHandle(metricStreamer)->getNotificationState() == Event::State::STATE_SIGNALED) {
                return Event::State::STATE_SIGNALED;
            }
        }
        return Event::State::STATE_INITIAL;
    }

    auto metricGroup = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup));
    bool reportsReady = metricGroup->waitForReports(0) == ZE_RESULT_SUCCESS;

    return reportsReady
               ? Event::State::STATE_SIGNALED
               : Event::State::STATE_INITIAL;
}

std::vector<zet_metric_streamer_handle_t> &OaMetricStreamerImp::getMetricStreamers() {
    return metricStreamers;
}

uint32_t OaMetricStreamerImp::getRequiredBufferSize(const uint32_t maxReportCount) const {
    DEBUG_BREAK_IF(rawReportSize == 0);
    uint32_t maxOaBufferReportCount = oaBufferSize / rawReportSize;

    // Trim report count if needed.
    const auto reportCount = std::min(maxOaBufferReportCount, maxReportCount);
    return reportCount * rawReportSize;
}

ze_result_t OaMetricGroupImp::openForDevice(Device *pDevice, zet_metric_streamer_desc_t &desc,
                                            zet_metric_streamer_handle_t *phMetricStreamer) {

    auto &metricSource = pDevice->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();

    *phMetricStreamer = nullptr;

    // Check whether metric streamer is already open.
    if (metricSource.getMetricStreamer() != nullptr) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    // metric streamer cannot be used with query simultaneously
    // (oa buffer cannot be shared).
    if (metricSource.getMetricsLibrary().getMetricQueryCount() > 0) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Unload metrics library if there are no active queries.
    // It will allow to open metric streamer. Query and streamer cannot be used
    // simultaneously since they use the same exclusive resource (oa buffer).
    if (metricSource.getMetricsLibrary().getInitializationState() == ZE_RESULT_SUCCESS) {
        metricSource.getMetricsLibrary().release();
    }

    // Check metric group sampling type.
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    getProperties(&metricGroupProperties);
    if ((metricGroupProperties.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) != ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Check whether metric group is activated.
    if (!metricSource.isMetricGroupActivated(toHandle())) {
        return ZE_RESULT_NOT_READY;
    }

    auto pMetricStreamer = new OaMetricStreamerImp();
    UNRECOVERABLE_IF(pMetricStreamer == nullptr);
    pMetricStreamer->initialize(pDevice->toHandle(), toHandle());

    const ze_result_t result = pMetricStreamer->startMeasurements(
        desc.notifyEveryNReports, desc.samplingPeriod);
    if (result == ZE_RESULT_SUCCESS) {
        metricSource.setMetricStreamer(pMetricStreamer);
    } else {
        delete pMetricStreamer;
        pMetricStreamer = nullptr;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    *phMetricStreamer = pMetricStreamer->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricGroupImp::streamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_streamer_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto pDevice = Device::fromHandle(hDevice);
    const auto pDeviceImp = static_cast<const DeviceImp *>(pDevice);

    if (pDeviceImp->metricContext->isImplicitScalingCapable()) {
        const uint32_t subDeviceCount = pDeviceImp->numSubDevices;
        auto pMetricStreamer = new OaMetricStreamerImp();
        UNRECOVERABLE_IF(pMetricStreamer == nullptr);

        auto &metricStreamers = pMetricStreamer->getMetricStreamers();
        metricStreamers.resize(subDeviceCount);

        for (uint32_t i = 0; i < subDeviceCount; i++) {

            auto metricGroupsSubDevice = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(getMetricGroups()[i]));
            result = metricGroupsSubDevice->openForDevice(pDeviceImp->subDevices[i], *desc, &metricStreamers[i]);
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
        result = openForDevice(pDevice, *desc, phMetricStreamer);
    }

    if (result == ZE_RESULT_SUCCESS) {
        OaMetricStreamerImp *metImp = static_cast<OaMetricStreamerImp *>(MetricStreamer::fromHandle(*phMetricStreamer));
        metImp->attachEvent(hNotificationEvent);
    }

    return result;
}

ze_result_t OaMetricStreamerImp::appendStreamerMarker(CommandList &commandList, uint32_t value) {
    auto &metricSource = commandList.getDevice()->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();
    return metricSource.appendMarker(commandList.toHandle(), hMetricGroup, value);
}

} // namespace L0
