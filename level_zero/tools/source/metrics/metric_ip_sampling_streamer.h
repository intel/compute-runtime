/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

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
                              const std::vector<MetricScopeImp *> &metricScopes,
                              uint32_t cachedMetricsCount,
                              std::vector<MetricImp *> &metricsInReport,
                              std::vector<uint32_t> &includedMetricIndexes)
        : MetricCalcOpImp(multidevice, metricScopes, metricsInReport, std::vector<MetricImp *>()),
          cachedMetricsCount(cachedMetricsCount),
          includedMetricIndexes(includedMetricIndexes) {}

    ~IpSamplingMetricCalcOpImp() override{};
    static ze_result_t create(bool isMultiDevice,
                              const std::vector<MetricScopeImp *> &metricScopes,
                              IpSamplingMetricSourceImp &metricSource,
                              zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
                              zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation);
    ze_result_t destroy() override;

    ze_result_t metricCalculateValues(const size_t rawDataSize, const uint8_t *pRawData,
                                      bool final, size_t *usedSize,
                                      uint32_t *pTotalMetricReportCount,
                                      zet_intel_metric_result_exp_t *pMetricResults) override;

    void fillStallDataMap(const size_t rawDataSize, const uint8_t *pRawData, size_t *processedSize,
                          L0GfxCoreHelper &l0GfxCoreHelper,
                          std::map<uint64_t, void *> &stallReportDataMap,
                          bool *dataOverflow);

  protected:
    ze_result_t metricCalculateValuesSingle(const size_t rawDataSize, const uint8_t *pRawData,
                                            uint32_t *pTotalMetricReportCount,
                                            L0::L0GfxCoreHelper &l0GfxCoreHelper,
                                            IpSamplingMetricGroupBase *metricGroupBase,
                                            bool getSize,
                                            size_t *usedSize,
                                            bool &dataOverflow,
                                            std::map<uint64_t, void *> &stallReportDataMap);
    ze_result_t metricCalculateValuesMulti(const size_t rawDataSize, const uint8_t *pRawData,
                                           uint32_t *pTotalMetricReportCount,
                                           L0::L0GfxCoreHelper &l0GfxCoreHelper,
                                           IpSamplingMetricGroupBase *metricGroupBase,
                                           bool getSize,
                                           uint32_t numSubDevices,
                                           size_t *usedSize,
                                           bool &dataOverflow,
                                           std::map<uint64_t, void *> &stallReportDataMap);

    uint32_t cachedMetricsCount = 0;
    std::vector<uint32_t> includedMetricIndexes{};
};

} // namespace L0
