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

    auto hwBufferSizeDesc = MetricSource::getHwBufferSizeDesc(static_cast<zet_base_desc_t *>(const_cast<void *>(desc->pNext)));
    if (hwBufferSizeDesc.has_value()) {
        // EUSS uses fixed max hw buffer size
        hwBufferSizeDesc.value()->sizeInBytes = source.getMetricOsInterface()->getRequiredBufferSize(UINT32_MAX);
    }
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

IpSamplingMetricCalcOpImp::IpSamplingMetricCalcOpImp(bool multidevice,
                                                     const std::vector<MetricScopeImp *> &metricScopesInCalcOp,
                                                     std::vector<MetricImp *> &metricsInReport,
                                                     const std::vector<MetricScopeImp *> &metricScopesInReport,
                                                     IpSamplingMetricSourceImp &metricSource,
                                                     std::vector<uint32_t> &includedMetricIndexes)
    : MetricCalcOpImp(multidevice, metricScopesInReport, metricsInReport, std::vector<MetricImp *>()),
      metricSource(metricSource), includedMetricIndexes(includedMetricIndexes), metricScopesInCalcOp(metricScopesInCalcOp) {

    // Create an IP data cache for each scope in the calcOp
    for (auto &scope : metricScopesInCalcOp) {
        auto scopeId = scope->getId();
        perScopeIpDataCaches[scopeId] = new std::map<uint64_t, void *>{};
        if (scope->isAggregated()) {
            isAggregateScopeIncluded = true;
            aggregateScopeId = scope->getId();
        }
    }

    const auto deviceImp = static_cast<DeviceImp *>(&(metricSource.getMetricDeviceContext().getDevice()));
    l0GfxCoreHelper = &(deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>());
}

void IpSamplingMetricCalcOpImp::clearScopesCaches() {
    for (auto &entry : perScopeIpDataCaches) {
        l0GfxCoreHelper->stallIpDataMapDeleteSumData(*entry.second);
        entry.second->clear();
    }
}

ze_result_t IpSamplingMetricCalcOpImp::destroy() {
    clearScopesCaches();
    for (auto &it : perScopeIpDataCaches) {
        delete it.second;
        it.second = nullptr;
    }

    perScopeIpDataCaches.clear();
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::create(bool isMultiDevice,
                                              IpSamplingMetricSourceImp &metricSource,
                                              zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
                                              zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation) {

    // There is only one valid metric group in IP sampling and time filtering is not supported
    // So only metrics handles are used to filter the metrics

    // The order of metrics in the report should be the same as the one in the HW report to optimize calculation
    uint32_t metricGroupCount = 1;
    zet_metric_group_handle_t hMetricGroup = {};
    if (auto ret = metricSource.metricGroupGet(&metricGroupCount, &hMetricGroup); ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    uint32_t metricCount = 0;
    MetricGroup::fromHandle(hMetricGroup)->metricGet(&metricCount, nullptr);
    std::vector<zet_metric_handle_t> hMetrics(metricCount);
    MetricGroup::fromHandle(hMetricGroup)->metricGet(&metricCount, hMetrics.data());
    std::vector<MetricImp *> includedMetrics = {};
    std::vector<uint32_t> includedMetricIndexes = {};

    for (uint32_t i = 0; i < metricCount; i++) {
        auto metric = static_cast<MetricImp *>(Metric::fromHandle(hMetrics[i]));
        if (pCalculationDesc->metricGroupCount > 0) {
            includedMetrics.push_back(metric);
            includedMetricIndexes.push_back(i);
        } else {
            auto metricsBegin = pCalculationDesc->phMetrics;
            auto metricsEnd = pCalculationDesc->phMetrics + pCalculationDesc->metricCount;
            if (std::find(metricsBegin, metricsEnd, hMetrics[i]) != metricsEnd) {
                includedMetrics.push_back(metric);
                includedMetricIndexes.push_back(i);
            }
        }
    }

    std::vector<MetricScopeImp *> metricScopes;
    for (uint32_t i = 0; i < pCalculationDesc->metricScopesCount; i++) {
        metricScopes.push_back(static_cast<MetricScopeImp *>(MetricScope::fromHandle(pCalculationDesc->phMetricScopes[i])));
    }

    // Create metricsInReport and corresponding scopes in metricScopesInReport
    std::vector<MetricImp *> metricsInReport = {};
    std::vector<MetricScopeImp *> metricScopesInReport = {};

    for (uint32_t scopeIndex = 0; scopeIndex < metricScopes.size(); scopeIndex++) {
        for (uint32_t metricIndex = 0; metricIndex < includedMetrics.size(); metricIndex++) {
            metricsInReport.push_back(includedMetrics[metricIndex]);
            metricScopesInReport.push_back(metricScopes[scopeIndex]);
        }
    }

    auto calcOp = new IpSamplingMetricCalcOpImp(isMultiDevice, metricScopes, metricsInReport,
                                                metricScopesInReport, metricSource, includedMetricIndexes);
    *phCalculationOperation = calcOp->toHandle();
    ze_result_t status = ZE_RESULT_SUCCESS;
    if ((pCalculationDesc->timeWindowsCount > 0) || (pCalculationDesc->timeAggregationWindow != 0)) {
        // Time filtering is not supported in IP sampling
        status = ZE_INTEL_RESULT_WARNING_TIME_PARAMS_IGNORED_EXP;
        METRICS_LOG_INFO("%s", "Time filtering is not supported in IP sampling, ignoring time windows and aggregation window");
    }

    return status;
}

ze_result_t IpSamplingMetricCalcOpImp::getSingleComputeScopeReportCount(const size_t rawDataSize, const uint8_t *pRawData,
                                                                        bool newData, uint32_t scopeId, uint32_t *pTotalMetricReportCount) {
    ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;
    auto ipSamplingCalculation = metricSource.ipSamplingCalculation.get();
    std::unordered_set<uint64_t> iPs{};
    if (newData) {
        status = ipSamplingCalculation->getIpsInRawData(rawDataSize, pRawData, iPs);
        if (status != ZE_RESULT_SUCCESS) {
            *pTotalMetricReportCount = 0;
            return status;
        }
    }

    *pTotalMetricReportCount = getUniqueIpCountForScope(scopeId, iPs);

    DEBUG_BREAK_IF(*pTotalMetricReportCount == 0);

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::getMultiScopeReportCount(const size_t rawDataSize, const uint8_t *pRawData,
                                                                bool newData, uint32_t *pTotalMetricReportCount) {

    ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;
    auto ipSamplingCalculation = metricSource.ipSamplingCalculation.get();

    // IPs are shared across sub-devices. So, if aggregated scope is provided, the number of reports is the total
    // number of unique IPs across all sub-devices data.
    // Otherwise, the number of reports is the number of IPs in the sub-device compute scope that has the most IPs, and
    // results will have invalid status for reports in compute scopes that have fewer IPs.

    if (newData) {
        std::unordered_set<uint64_t> iPs{};
        std::vector<uint32_t> subDeviceIndexes{};
        std::vector<MetricScopeImp *>::iterator it = metricScopesInCalcOp.begin();
        if (isAggregateScopeIncluded) {
            DeviceImp *deviceImp = static_cast<DeviceImp *>(&metricSource.getMetricDeviceContext().getDevice());
            uint32_t subDeviceCount = deviceImp->numSubDevices;
            std::vector<ze_device_handle_t> subDevices(subDeviceCount);
            deviceImp->getSubDevices(&subDeviceCount, subDevices.data());
            for (auto &subDeviceHandle : subDevices) {
                auto neoSubDevice = static_cast<NEO::SubDevice *>(Device::fromHandle(subDeviceHandle)->getNEODevice());
                subDeviceIndexes.push_back(neoSubDevice->getSubDeviceIndex());
            }
        } else {
            for (auto &scope : metricScopesInCalcOp) {
                subDeviceIndexes.push_back(scope->getComputeSubDeviceIndex());
            }
        }

        for (auto &subdevIndex : subDeviceIndexes) {
            status = ipSamplingCalculation->getIpsInRawDataForSubDevIndex(rawDataSize, pRawData, subdevIndex, iPs);
            if (status != ZE_RESULT_SUCCESS) {
                *pTotalMetricReportCount = 0;
                return status;
            }

            if (!isAggregateScopeIncluded) {
                *pTotalMetricReportCount = std::max(*pTotalMetricReportCount, getUniqueIpCountForScope((*it)->getId(), iPs));
                it++;
                iPs.clear();
            }
        }

        if (isAggregateScopeIncluded) {
            *pTotalMetricReportCount = getUniqueIpCountForScope(aggregateScopeId, iPs);
        }
    } else {
        *pTotalMetricReportCount = getLargestCacheSize();
    }

    DEBUG_BREAK_IF(*pTotalMetricReportCount == 0);
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::updateCacheForSingleScope(const size_t rawDataSize, const uint8_t *pRawData,
                                                                 bool newData, std::map<uint64_t, void *> &reportDataMap, bool &dataOverflow) {
    if (newData) {
        ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;
        auto ipSamplingCalculation = metricSource.ipSamplingCalculation.get();

        status = ipSamplingCalculation->updateStallDataMapFromData(rawDataSize, pRawData,
                                                                   reportDataMap, &dataOverflow);
        if (status != ZE_RESULT_SUCCESS) {
            METRICS_LOG_ERR("%s", "Failed to update stall data map");
            return status;
        }
    }

    DEBUG_BREAK_IF(reportDataMap.empty());
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::updateCachesForMultiScopes(const size_t rawDataSize, const uint8_t *pRawData,
                                                                  bool newData, bool &dataOverflow) {

    // Process new data only once per sub-device.
    // Aggregated scope update needs metrics only from new data. So:
    //      a) Save sub-devices data in temporary caches for each compute scope
    //      b) Update aggregated scope temporary caches data.
    // For compute scopes use the temporary caches to update each compute scope cache accordingly

    if (newData) {
        MetricScopeImp *aggregatedScope = nullptr;
        std::map<uint32_t, std::map<uint64_t, void *> *> computeNewIpsCaches{};
        std::vector<uint32_t> subDevIndexesForComputeScopes{};
        ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;

        auto traverseDataAndUpdateCache = [this, rawDataSize, pRawData, newData, &dataOverflow](uint32_t subDeviceIndex, std::map<uint64_t, void *> &reportDataMap) -> ze_result_t {
            auto processedSize = 0u;
            ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
            while (processedSize < rawDataSize) {
                auto processMetricData = pRawData + processedSize;
                auto header = reinterpret_cast<const IpSamplingMultiDevDataHeader *>(processMetricData);
                processedSize += sizeof(IpSamplingMultiDevDataHeader) + header->rawDataSize;
                if (header->setIndex != subDeviceIndex) {
                    continue;
                }

                result = updateCacheForSingleScope(header->rawDataSize,
                                                   (processMetricData + sizeof(IpSamplingMultiDevDataHeader)),
                                                   newData, reportDataMap, dataOverflow);
                if (result != ZE_RESULT_SUCCESS) {
                    return result;
                }
            }
            return ZE_RESULT_SUCCESS;
        };

        auto clearTempCaches = [&computeNewIpsCaches, this]() {
            for (auto &it : computeNewIpsCaches) {
                l0GfxCoreHelper->stallIpDataMapDeleteSumData(*it.second);
                it.second->clear();
                delete it.second;
            }
            computeNewIpsCaches.clear();
        };

        for (auto &scope : metricScopesInCalcOp) {
            if (!scope->isAggregated()) {
                uint32_t subDeviceIndex = scope->getComputeSubDeviceIndex();
                subDevIndexesForComputeScopes.push_back(subDeviceIndex);
                // Temporary cache for new data per compute scope to allow updating aggregated and compute cached independently
                computeNewIpsCaches[scope->getId()] = new std::map<uint64_t, void *>{};
                status = traverseDataAndUpdateCache(subDeviceIndex, *computeNewIpsCaches[scope->getId()]);
                if (status != ZE_RESULT_SUCCESS) {
                    clearTempCaches();
                    return status;
                }
            } else {
                aggregatedScope = scope;
            }
        }

        if (aggregatedScope) {
            // Process data for the sub-devices not already processed above.
            std::map<uint64_t, void *> *aggregatedScopeCache = perScopeIpDataCaches[aggregatedScope->getId()];
            DeviceImp *deviceImp = static_cast<DeviceImp *>(&metricSource.getMetricDeviceContext().getDevice());
            uint32_t subDeviceCount = deviceImp->numSubDevices;
            std::vector<ze_device_handle_t> subDevices(subDeviceCount);
            deviceImp->getSubDevices(&subDeviceCount, subDevices.data());

            std::vector<uint32_t> pendingSubDeviceIndexes{};
            for (auto &subDeviceHandle : subDevices) {
                auto neoSubDevice = static_cast<NEO::SubDevice *>(Device::fromHandle(subDeviceHandle)->getNEODevice());
                uint32_t subdevIndex = neoSubDevice->getSubDeviceIndex();
                if (std::find(subDevIndexesForComputeScopes.begin(), subDevIndexesForComputeScopes.end(), subdevIndex) == subDevIndexesForComputeScopes.end()) {
                    pendingSubDeviceIndexes.push_back(subdevIndex);
                }
            }

            for (auto &subDeviceIndex : pendingSubDeviceIndexes) {
                status = traverseDataAndUpdateCache(subDeviceIndex, *aggregatedScopeCache);
                if (status != ZE_RESULT_SUCCESS) {
                    clearTempCaches();
                    return status;
                }
            }

            // Update aggregated scope cache with Ips from new data in sub-devices caches.
            for (auto &it : computeNewIpsCaches) {
                l0GfxCoreHelper->stallIpDataMapUpdateFromMap(*it.second, *aggregatedScopeCache);
            }
        }

        // Update each compute scope cache with its data and free the temporary caches
        for (auto &it : computeNewIpsCaches) {
            l0GfxCoreHelper->stallIpDataMapUpdateFromMap(*it.second, *perScopeIpDataCaches[it.first]);
        }

        clearTempCaches();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricCalcOpImp::metricCalculateValues(const size_t rawDataSize, const uint8_t *pRawData,
                                                             bool lastCall, size_t *usedSize,
                                                             uint32_t *pTotalMetricReportCount,
                                                             zet_intel_metric_result_exp_t *pMetricResults) {
    *usedSize = 0;
    uint32_t metricReportCount = 0;
    bool dataOverflow = false;
    auto ipSamplingCalculation = metricSource.ipSamplingCalculation.get();
    bool getSize = (*pTotalMetricReportCount == 0);
    bool newData = false; // Track if there is fresh new raw data that requires updating caches
    uint64_t dataSize = rawDataSize;
    const uint8_t *rawDataStart = pRawData;

    ze_result_t status = ZE_RESULT_SUCCESS;

    if (areAllCachesEmpty()) {
        // All data is new: user asked to calculate all results available in the raw data. So, all caches are empty
        newData = true;
    } else if (rawDataSize > processedSize) {
        // Previous call user requested fewer results than available. So, algo cached pending results and
        // processed size = input size - rawReportSize  because returned used size = rawReportSize.
        // Then user is expected to move pRawData by rawReportSize. If data gets appended user must update
        // new size accordingly.
        newData = true;
        rawDataStart += processedSize;
        dataSize = rawDataSize - processedSize;
    }

    if (newData) { // only check on new data. Otherwise, cached values are guaranteed to be valid
        isMultiDeviceData = IpSamplingCalculation::isMultiDeviceCaptureData(dataSize, rawDataStart);
    }

    if (!isMultiDevice) {
        if (isMultiDeviceData) {
            METRICS_LOG_ERR("%s", "Cannot use root device raw data in a sub-device calculation operation handle");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        uint32_t scopeId = metricScopesInCalcOp[0]->getId(); // Sub-devices calcOp can only have single scope
        DEBUG_BREAK_IF(metricScopesInCalcOp[0]->isAggregated());
        DEBUG_BREAK_IF(scopeId != 0);

        if (getSize) {
            return getSingleComputeScopeReportCount(dataSize, rawDataStart, newData, scopeId, pTotalMetricReportCount);
        }

        status = updateCacheForSingleScope(dataSize, rawDataStart, newData, *perScopeIpDataCaches[scopeId], dataOverflow);
        if (status != ZE_RESULT_SUCCESS) {
            clearScopesCaches();
            METRICS_LOG_ERR("%s", "Failed to update stall data");
            return status;
        }

        metricReportCount = std::min<uint32_t>(*pTotalMetricReportCount, getLargestCacheSize());
        ipSamplingCalculation->stallDataMapToMetricResults(*getScopeCache(scopeId), metricReportCount, includedMetricIndexes, pMetricResults);
    } else {
        if (!isMultiDeviceData) {
            METRICS_LOG_ERR("%s", "Cannot use sub-device raw data in a root device calculation operation handle");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        if (getSize) {
            return getMultiScopeReportCount(dataSize, rawDataStart, newData, pTotalMetricReportCount);
        }

        status = updateCachesForMultiScopes(dataSize, rawDataStart, newData, dataOverflow);
        if (status != ZE_RESULT_SUCCESS) {
            clearScopesCaches();
            METRICS_LOG_ERR("%s", "Failed to update stall data map");
            return status;
        }

        metricReportCount = std::min<uint32_t>(*pTotalMetricReportCount, getLargestCacheSize());
        ipSamplingCalculation->multiDataMapToMetricResults(perScopeIpDataCaches, metricReportCount, includedMetricIndexes, pMetricResults);
    }
    // Update with the actual number of reports calculated
    *pTotalMetricReportCount = metricReportCount;

    if (lastCall || areAllCachesEmpty()) {
        clearScopesCaches();
        *usedSize = rawDataSize;
        processedSize = 0;
    } else {
        *usedSize = IpSamplingCalculation::rawReportSize;
        processedSize = rawDataSize - IpSamplingCalculation::rawReportSize;
    }

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
    const auto numSubDevices = subDeviceMetricGroup.size();
    subDeviceStreamers.reserve(numSubDevices);

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

    auto hwBufferSizeDesc = MetricSource::getHwBufferSizeDesc(static_cast<zet_base_desc_t *>(const_cast<void *>(desc->pNext)));
    if (hwBufferSizeDesc.has_value()) {
        auto device = Device::fromHandle(hDevice);
        IpSamplingMetricSourceImp &source = device->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        // EUSS uses fixed max hw buffer size
        hwBufferSizeDesc.value()->sizeInBytes = source.getMetricOsInterface()->getRequiredBufferSize(UINT32_MAX) * numSubDevices;
    }
    return result;
}

ze_result_t MultiDeviceIpSamplingMetricStreamerImp::readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) {

    const int32_t totalHeaderSize = static_cast<int32_t>(sizeof(IpSamplingMultiDevDataHeader) * subDeviceStreamers.size());
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
        auto header = reinterpret_cast<IpSamplingMultiDevDataHeader *>(pCurrRawData);
        pCurrRawData += sizeof(IpSamplingMultiDevDataHeader);

        result = streamer->readData(maxReportCount, &currRawDataSize, pCurrRawData);
        if (result != ZE_RESULT_SUCCESS) {
            *pRawDataSize = 0;
            return result;
        }

        // Update to header
        memset(header, 0, sizeof(IpSamplingMultiDevDataHeader));
        header->magic = IpSamplingMultiDevDataHeader::magicValue;
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
