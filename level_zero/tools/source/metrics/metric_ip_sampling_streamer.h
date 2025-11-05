/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

#include <unordered_set>
namespace L0 {

class IpSamplingMetricSourceImp;
struct IpSamplingMetricImp;
struct IpSamplingMetricGroupBase;
struct IpSamplingMetricGroupImp;
class L0GfxCoreHelper;

struct IpSamplingMetricStreamerBase : public MetricStreamer {
    ze_result_t appendStreamerMarker(CommandList &commandList, uint32_t value) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
};

struct IpSamplingMetricStreamerImp : public IpSamplingMetricStreamerBase {

    IpSamplingMetricStreamerImp(IpSamplingMetricSourceImp &ipSamplingSource) : ipSamplingSource(ipSamplingSource) {}
    ~IpSamplingMetricStreamerImp() override{};
    ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) override;
    ze_result_t close() override;
    Event::State getNotificationState() override;
    uint32_t getMaxSupportedReportCount();

  protected:
    IpSamplingMetricSourceImp &ipSamplingSource;
};

struct MultiDeviceIpSamplingMetricStreamerImp : public IpSamplingMetricStreamerBase {

    MultiDeviceIpSamplingMetricStreamerImp(std::vector<IpSamplingMetricStreamerImp *> subDeviceStreamers) : subDeviceStreamers(subDeviceStreamers) {}
    ~MultiDeviceIpSamplingMetricStreamerImp() override = default;
    ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize, uint8_t *pRawData) override;
    ze_result_t close() override;
    Event::State getNotificationState() override;

  protected:
    std::vector<IpSamplingMetricStreamerImp *> subDeviceStreamers = {};
};

struct IpSamplingMetricCalcOpImp : public MetricCalcOpImp {
    IpSamplingMetricCalcOpImp(bool multidevice,
                              const std::vector<MetricScopeImp *> &metricScopesInCalcOp,
                              std::vector<MetricImp *> &metricsInReport,
                              const std::vector<MetricScopeImp *> &metricScopesInReport,
                              IpSamplingMetricSourceImp &metricSource,
                              std::vector<uint32_t> &includedMetricIndexes);
    ~IpSamplingMetricCalcOpImp() override{};

    static ze_result_t create(bool isMultiDevice,
                              IpSamplingMetricSourceImp &metricSource,
                              zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
                              zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation);
    ze_result_t destroy() override;

    ze_result_t metricCalculateValues(const size_t rawDataSize, const uint8_t *pRawData,
                                      bool lastCall, size_t *usedSize,
                                      uint32_t *pTotalMetricReportCount,
                                      zet_intel_metric_result_exp_t *pMetricResults) override;

  protected:
    IpSamplingMetricSourceImp &metricSource;
    std::vector<uint32_t> includedMetricIndexes{};
    std::vector<MetricScopeImp *> metricScopesInCalcOp{};
    bool isMultiDeviceData = false;
    bool isAggregateScopeIncluded = false;
    uint32_t aggregateScopeId = 0; // valid if isAggregateScopeIncluded is true

    std::map<uint32_t, std::map<uint64_t, void *> *> perScopeIpDataCaches{};
    size_t processedSize = 0;
    L0::L0GfxCoreHelper *l0GfxCoreHelper = nullptr;

    ze_result_t getSingleComputeScopeReportCount(const size_t rawDataSize, const uint8_t *pRawData,
                                                 bool newData, uint32_t scopeId, uint32_t *pTotalMetricReportCount);
    ze_result_t getMultiScopeReportCount(const size_t rawDataSize, const uint8_t *pRawData,
                                         bool newData, uint32_t *pTotalMetricReportCount);
    ze_result_t updateCacheForSingleScope(const size_t rawDataSize, const uint8_t *pRawData,
                                          bool newData, std::map<uint64_t, void *> &reportDataMap, bool &dataOverflow);
    ze_result_t updateCachesForMultiScopes(const size_t rawDataSize, const uint8_t *pRawData,
                                           bool newData, bool &dataOverflow);
    bool isScopeCacheEmpty(uint32_t scopeId) {
        return perScopeIpDataCaches[scopeId]->empty();
    }

    bool areAllCachesEmpty() {
        for (const auto &it : perScopeIpDataCaches) {
            if (!isScopeCacheEmpty(it.first)) {
                return false;
            }
        }
        return true;
    }

    void clearScopesCaches();

    // Get unique IPs count from new data and scope cache
    uint32_t getUniqueIpCountForScope(uint32_t scopeId, std::unordered_set<uint64_t> &ips) {
        for (const auto &entry : *perScopeIpDataCaches[scopeId]) {
            ips.insert(entry.first);
        }
        return static_cast<uint32_t>(ips.size());
    }

    std::map<uint64_t, void *> *getScopeCache(uint32_t scopeId) {
        return perScopeIpDataCaches[scopeId];
    }

    uint32_t getLargestCacheSize() {
        uint32_t maxSize = 0;
        for (auto &it : perScopeIpDataCaches) {
            maxSize = std::max(maxSize, static_cast<uint32_t>(it.second->size()));
        }
        return maxSize;
    }
};

} // namespace L0
