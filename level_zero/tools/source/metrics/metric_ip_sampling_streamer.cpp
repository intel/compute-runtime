/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"

#include "shared/source/execution_environment/root_device_environment.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include <level_zero/zet_api.h>

#include <set>
#include <string.h>

namespace L0 {

ze_result_t IpSamplingMetricGroupImp::streamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_streamer_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {

    auto device = Device::fromHandle(hDevice);

    // Check whether metric group is activated.
    IpSamplingMetricSourceImp &source = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
    if (!source.isMetricGroupActivated(this->toHandle())) {
        return ZE_RESULT_NOT_READY;
    }

    // Check whether metric streamer is already open.
    if (source.pActiveStreamer != nullptr) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    auto pStreamerImp = new IpSamplingMetricStreamerImp(source);
    UNRECOVERABLE_IF(pStreamerImp == nullptr);

    const ze_result_t result = source.getMetricOsInterface()->startMeasurement(desc->notifyEveryNReports, desc->samplingPeriod);
    if (result == ZE_RESULT_SUCCESS) {
        source.pActiveStreamer = pStreamerImp;
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

uint32_t IpSamplingMetricStreamerImp::getMaxSupportedReportCount() {
    const auto unitReportSize = ipSamplingSource.getMetricOsInterface()->getUnitReportSize();
    UNRECOVERABLE_IF(unitReportSize == 0);
    return ipSamplingSource.getMetricOsInterface()->getRequiredBufferSize(UINT32_MAX) / unitReportSize;
}

ze_result_t IpSamplingMetricCalcOpImp::create(IpSamplingMetricSourceImp &metricSource,
                                              zet_intel_metric_calculate_exp_desc_t *pCalculateDesc,
                                              bool isMultiDevice,
                                              zet_intel_metric_calculate_operation_exp_handle_t *phCalculateOperation) {

    // There is only one valid metric group in IP sampling and time filtering is not supported
    // So only metrics handles are used to filter the metrics

    // avoid duplicates
    std::set<zet_metric_handle_t> uniqueMetricHandles(pCalculateDesc->phMetrics, pCalculateDesc->phMetrics + pCalculateDesc->metricCount);

    // The order of metrics in the report should be the same as the one in the HW report to optimize calculation
    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t hMetricGroup = {};
    metricSource.metricGroupGet(&metricGroupCount, &hMetricGroup);
    uint32_t metricCount = 0;
    MetricGroup::fromHandle(hMetricGroup)->metricGet(&metricCount, nullptr);
    std::vector<zet_metric_handle_t> hMetrics(metricCount);
    MetricGroup::fromHandle(hMetricGroup)->metricGet(&metricCount, hMetrics.data());
    std::vector<MetricImp *> inputMetricsInReport = {};
    std::vector<uint32_t> includedMetricIndexes = {};

    for (uint32_t i = 0; i < metricCount; i++) {
        auto metric = static_cast<MetricImp *>(Metric::fromHandle(hMetrics[i]));
        if (pCalculateDesc->metricGroupCount > 0) {
            inputMetricsInReport.push_back(metric);
            includedMetricIndexes.push_back(i);
        } else {
            if (uniqueMetricHandles.find(hMetrics[i]) != uniqueMetricHandles.end()) {
                inputMetricsInReport.push_back(metric);
                includedMetricIndexes.push_back(i);
            }
        }
    }

    auto calcOp = new IpSamplingMetricCalcOpImp(static_cast<uint32_t>(hMetrics.size()),
                                                inputMetricsInReport, includedMetricIndexes, isMultiDevice);
    *phCalculateOperation = calcOp->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::getReportFormat(uint32_t *pCount, zet_metric_handle_t *phMetrics) {

    if (*pCount == 0) {
        *pCount = metricsInReportCount;
        return ZE_RESULT_SUCCESS;
    } else if (*pCount < metricsInReportCount) {
        METRICS_LOG_ERR("%s", "Metric can't be smaller than report size");
        *pCount = 0;
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *pCount = metricsInReportCount;
    for (uint32_t index = 0; index < metricsInReportCount; index++) {
        phMetrics[index] = metricsInReport[index]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

void IpSamplingMetricCalcOpImp::fillStallDataMap(const size_t rawDataSize, const uint8_t *pRawData, size_t *processedSize,
                                                 L0::L0GfxCoreHelper &l0GfxCoreHelper,
                                                 std::map<uint64_t, void *> &stallReportDataMap,
                                                 bool *dataOVerflow,
                                                 bool allowInterrupt,
                                                 uint32_t requestedReportCount) {

    const uint32_t rawReportSize = IpSamplingMetricGroupBase::rawReportSize;
    *processedSize = 0;

    uint32_t processedReportCount = 0;
    const uint8_t *dataToProcess = pRawData;
    do {

        *dataOVerflow |= l0GfxCoreHelper.stallIpDataMapUpdate(stallReportDataMap, dataToProcess);
        *processedSize += rawReportSize;
        dataToProcess += rawReportSize;
        // Number of reports is defined by the number of IPs in the raw data.
        processedReportCount = static_cast<uint32_t>(stallReportDataMap.size());
    } while ((*processedSize < rawDataSize) &&
             (!allowInterrupt || (processedReportCount < requestedReportCount)));

    return;
}

ze_result_t IpSamplingMetricCalcOpImp::metricCalculateValuesSingle(const size_t rawDataSize, size_t *pOffset, const uint8_t *pRawData,
                                                                   uint32_t *pTotalMetricReportCount,
                                                                   L0::L0GfxCoreHelper &l0GfxCoreHelper,
                                                                   IpSamplingMetricGroupBase *metricGroupBase,
                                                                   bool getSize,
                                                                   bool &dataOverflow,
                                                                   std::map<uint64_t, void *> &stallReportDataMap) {
    ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t resultCount = 0;
    *pOffset = 0;

    status = static_cast<IpSamplingMetricGroupImp *>(metricGroupBase)->getCalculatedMetricCount(pRawData, rawDataSize, resultCount);
    if (status != ZE_RESULT_SUCCESS) {
        *pTotalMetricReportCount = 0;
        return status;
    }

    uint32_t rawDataReportCount = resultCount / cachedMetricsCount;

    if (getSize) {
        *pTotalMetricReportCount = rawDataReportCount;
        return ZE_RESULT_SUCCESS;
    }

    // Only allow interrupting when requesting less reports than available since it affects the bytes
    // processed: report count can be reached before processing all data.
    bool allowInterrupt = false;
    if (*pTotalMetricReportCount < rawDataReportCount) {
        allowInterrupt = true;
    }

    fillStallDataMap(rawDataSize, pRawData, pOffset, l0GfxCoreHelper, stallReportDataMap,
                     &dataOverflow, allowInterrupt, *pTotalMetricReportCount);

    *pTotalMetricReportCount = static_cast<uint32_t>(stallReportDataMap.size());
    return status;
}

ze_result_t IpSamplingMetricCalcOpImp::metricCalculateValuesMulti(const size_t rawDataSize, size_t *pOffset, const uint8_t *pRawData,
                                                                  uint32_t *pTotalMetricReportCount,
                                                                  L0::L0GfxCoreHelper &l0GfxCoreHelper,
                                                                  IpSamplingMetricGroupBase *metricGroupBase,
                                                                  bool getSize,
                                                                  uint32_t numSubDevices,
                                                                  bool &dataOverflow,
                                                                  std::map<uint64_t, void *> &stallReportDataMap) {
    ze_result_t status = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    return status;
}

ze_result_t IpSamplingMetricCalcOpImp::metricCalculateValues(const size_t rawDataSize, size_t *pOffset, const uint8_t *pRawData,
                                                             uint32_t *pTotalMetricReportCount,
                                                             zet_intel_metric_result_exp_t *pMetricResults) {
    ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;
    bool dataOverflow = false;
    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t hMetricGroup = {};
    metricsInReport[0]->getMetricSource().metricGroupGet(&metricGroupCount, &hMetricGroup);
    bool isMultiDeviceData = IpSamplingMetricGroupBase::isMultiDeviceCaptureData(rawDataSize, pRawData);

    IpSamplingMetricGroupBase *metricGroupBase = static_cast<IpSamplingMetricGroupBase *>(MetricGroup::fromHandle(hMetricGroup));
    DeviceImp *deviceImp = static_cast<DeviceImp *>(&metricGroupBase->getMetricSource().getMetricDeviceContext().getDevice());
    L0::L0GfxCoreHelper &l0GfxCoreHelper = deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    pRawData += *pOffset; // Input Offset

    bool getSize = (*pTotalMetricReportCount == 0);
    std::map<uint64_t, void *> stallReportDataMap;

    if (!isMultiDevice) {
        if (isMultiDeviceData) {
            METRICS_LOG_ERR("%s", "Cannot use root device raw data in a sub-device calculate operation handle");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        status = metricCalculateValuesSingle(rawDataSize, pOffset, pRawData, pTotalMetricReportCount,
                                             l0GfxCoreHelper, metricGroupBase, getSize, dataOverflow, stallReportDataMap);
    } else {
        if (!isMultiDeviceData) {
            METRICS_LOG_ERR("%s", "Cannot use sub-device raw data in a root device calculate operation handle");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        status = metricCalculateValuesMulti(rawDataSize, pOffset, pRawData, pTotalMetricReportCount,
                                            l0GfxCoreHelper, metricGroupBase, getSize, deviceImp->numSubDevices,
                                            dataOverflow, stallReportDataMap);
    }

    if ((status != ZE_RESULT_SUCCESS) || (getSize)) {
        return status;
    }

    std::vector<zet_typed_value_t> ipDataValues;
    uint32_t i = 0;
    for (auto it = stallReportDataMap.begin(); it != stallReportDataMap.end(); ++it) {
        l0GfxCoreHelper.stallSumIpDataToTypedValues(it->first, it->second, ipDataValues);
        for (uint32_t j = 0; j < includedMetricIndexes.size(); j++) {
            (pMetricResults + i)->value = ipDataValues[includedMetricIndexes[j]].value;
            (pMetricResults + i)->resultStatus = ZET_INTEL_METRIC_CALCULATE_EXP_RESULT_VALID;
            i++;
        }
        ipDataValues.clear();
    }
    l0GfxCoreHelper.stallIpDataMapDelete(stallReportDataMap);
    stallReportDataMap.clear();

    return dataOverflow ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::streamerOpen(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_metric_streamer_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_streamer_handle_t *phMetricStreamer) {

    ze_result_t result = ZE_RESULT_SUCCESS;

    std::vector<IpSamplingMetricStreamerImp *> subDeviceStreamers = {};
    subDeviceStreamers.reserve(subDeviceMetricGroup.size());

    // Open SubDevice Streamers
    for (auto &metricGroup : subDeviceMetricGroup) {
        zet_metric_streamer_handle_t subDeviceMetricStreamer = {};
        zet_device_handle_t hSubDevice = metricGroup->getMetricSource().getMetricDeviceContext().getDevice().toHandle();
        result = metricGroup->streamerOpen(hContext, hSubDevice, desc, nullptr, &subDeviceMetricStreamer);
        if (result != ZE_RESULT_SUCCESS) {
            closeSubDeviceStreamers(subDeviceStreamers);
            return result;
        }
        subDeviceStreamers.push_back(static_cast<IpSamplingMetricStreamerImp *>(MetricStreamer::fromHandle(subDeviceMetricStreamer)));
    }

    auto pStreamerImp = new MultiDeviceIpSamplingMetricStreamerImp(subDeviceStreamers);
    UNRECOVERABLE_IF(pStreamerImp == nullptr);

    pStreamerImp->attachEvent(hNotificationEvent);
    *phMetricStreamer = pStreamerImp->toHandle();
    return result;
}

ze_result_t MultiDeviceIpSamplingMetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) {

    const int32_t totalHeaderSize = static_cast<int32_t>(sizeof(IpSamplingMetricDataHeader) * subDeviceStreamers.size());
    // Find single report size
    size_t singleReportSize = 0;
    subDeviceStreamers[0]->readData(1, &singleReportSize, nullptr);

    // Trim report count to the maximum possible report count
    const uint32_t maxSupportedReportCount = subDeviceStreamers[0]->getMaxSupportedReportCount() *
                                             static_cast<uint32_t>(subDeviceStreamers.size());
    maxReportCount = std::min(maxSupportedReportCount, maxReportCount);

    if (*pRawDataSize == 0) {
        *pRawDataSize = singleReportSize * maxReportCount;
        *pRawDataSize += totalHeaderSize;
        return ZE_RESULT_SUCCESS;
    }

    // Remove header size from actual data size
    size_t calcRawDataSize = std::max<int32_t>(0, static_cast<int32_t>(*pRawDataSize - totalHeaderSize));

    // Recalculate maximum possible report count for the raw data size
    calcRawDataSize = std::min(calcRawDataSize, singleReportSize * maxReportCount);
    maxReportCount = static_cast<uint32_t>(calcRawDataSize / singleReportSize);
    uint8_t *pCurrRawData = pRawData;
    size_t currRawDataSize = calcRawDataSize;

    ze_result_t result = ZE_RESULT_SUCCESS;

    for (uint32_t index = 0; index < subDeviceStreamers.size(); index++) {
        auto &streamer = subDeviceStreamers[index];

        // Get header address
        auto header = reinterpret_cast<IpSamplingMetricDataHeader *>(pCurrRawData);
        pCurrRawData += sizeof(IpSamplingMetricDataHeader);

        result = streamer->readData(maxReportCount, &currRawDataSize, pCurrRawData);
        if (result != ZE_RESULT_SUCCESS) {
            *pRawDataSize = 0;
            return result;
        }

        // Update to header
        memset(header, 0, sizeof(IpSamplingMetricDataHeader));
        header->magic = IpSamplingMetricDataHeader::magicValue;
        header->rawDataSize = static_cast<uint32_t>(currRawDataSize);
        header->setIndex = index;

        calcRawDataSize -= currRawDataSize;
        pCurrRawData += currRawDataSize;

        // Check whether memory available for next read
        if (calcRawDataSize < singleReportSize) {
            break;
        }
        maxReportCount -= static_cast<uint32_t>(currRawDataSize / singleReportSize);
        currRawDataSize = calcRawDataSize;
    }

    *pRawDataSize = pCurrRawData - pRawData;
    return result;
}

ze_result_t MultiDeviceIpSamplingMetricStreamerImp::close() {

    ze_result_t result = ZE_RESULT_SUCCESS;
    for (auto &streamer : subDeviceStreamers) {
        result = streamer->close();
        if (result != ZE_RESULT_SUCCESS) {
            break;
        }
    }

    subDeviceStreamers.clear();
    detachEvent();
    delete this;
    return result;
}

Event::State MultiDeviceIpSamplingMetricStreamerImp::getNotificationState() {

    Event::State state = Event::State::STATE_INITIAL;
    for (auto &streamer : subDeviceStreamers) {
        state = streamer->getNotificationState();
        if (state == Event::State::STATE_SIGNALED) {
            break;
        }
    }

    return state;
}

} // namespace L0
