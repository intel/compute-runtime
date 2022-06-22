/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"

namespace L0 {

struct IpSamplingMetricImp;
struct IpSamplingMetricGroupImp;
struct IpSamplingMetricStreamerImp;

class IpSamplingMetricSourceImp : public MetricSource {

  public:
    IpSamplingMetricSourceImp(const MetricDeviceContext &metricDeviceContext);
    ~IpSamplingMetricSourceImp() override = default;
    void enable() override;
    bool isAvailable() override;
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) override;
    ze_result_t appendMetricMemoryBarrier(CommandList &commandList) override;
    void setMetricOsInterface(std::unique_ptr<MetricIpSamplingOsInterface> &metricOsInterface);
    static std::unique_ptr<IpSamplingMetricSourceImp> create(const MetricDeviceContext &metricDeviceContext);
    MetricIpSamplingOsInterface *getMetricOsInterface() { return metricOsInterface.get(); }
    IpSamplingMetricStreamerImp *pActiveStreamer = nullptr;
    const MetricDeviceContext &getMetricDeviceContext() const { return metricDeviceContext; }

  protected:
    void cacheMetricGroup();
    bool isEnabled = false;

    const MetricDeviceContext &metricDeviceContext;
    std::unique_ptr<MetricIpSamplingOsInterface> metricOsInterface = nullptr;
    std::unique_ptr<MetricGroup> cachedMetricGroup = nullptr;
};

typedef struct StallSumIpData {
    uint64_t activeCount;
    uint64_t otherCount;
    uint64_t controlCount;
    uint64_t pipeStallCount;
    uint64_t sendCount;
    uint64_t distAccCount;
    uint64_t sbidCount;
    uint64_t syncCount;
    uint64_t instFetchCount;
} StallSumIpData_t;

typedef std::map<uint64_t, StallSumIpData_t> StallSumIpDataMap_t;

struct IpSamplingMetricGroupBase : public MetricGroup {
    bool activate() override { return true; }
    bool deactivate() override { return true; };
    ze_result_t metricQueryPoolCreate(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        const zet_metric_query_pool_desc_t *desc,
        zet_metric_query_pool_handle_t *phMetricQueryPool) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
};

struct IpSamplingMetricGroupImp : public IpSamplingMetricGroupBase {
    IpSamplingMetricGroupImp(IpSamplingMetricSourceImp &metricSource, std::vector<IpSamplingMetricImp> &metrics);
    ~IpSamplingMetricGroupImp() override = default;

    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override;
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                      const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pMetricValues) override;
    ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                         const uint8_t *pRawData, uint32_t *pSetCount,
                                         uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                         zet_typed_value_t *pMetricValues) override;
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override;
    ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) override;
    static std::unique_ptr<IpSamplingMetricGroupImp> create(IpSamplingMetricSourceImp &metricSource,
                                                            std::vector<IpSamplingMetricImp> &ipSamplingMetrics);
    IpSamplingMetricSourceImp &getMetricSource() { return metricSource; }

  private:
    std::vector<std::unique_ptr<IpSamplingMetricImp>> metrics = {};
    zet_metric_group_properties_t properties = {};
    ze_result_t getCalculatedMetricCount(const size_t rawDataSize, uint32_t &metricValueCount);
    ze_result_t getCalculatedMetricValues(const zet_metric_group_calculation_type_t type, const size_t rawDataSize, const uint8_t *pRawData,
                                          uint32_t &metricValueCount,
                                          zet_typed_value_t *pCalculatedData);
    bool stallIpDataMapUpdate(StallSumIpDataMap_t &, const uint8_t *pRawIpData);
    void stallSumIpDataToTypedValues(uint64_t ip, StallSumIpData_t &sumIpData, std::vector<zet_typed_value_t> &ipDataValues);
    IpSamplingMetricSourceImp &metricSource;
};

struct MultiDeviceIpSamplingMetricGroupImp : public IpSamplingMetricGroupBase {

    MultiDeviceIpSamplingMetricGroupImp(std::vector<IpSamplingMetricGroupImp *> &subDeviceMetricGroup) : subDeviceMetricGroup(subDeviceMetricGroup){};
    ~MultiDeviceIpSamplingMetricGroupImp() override = default;
    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override;
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                      const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pMetricValues) override;
    ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                         const uint8_t *pRawData, uint32_t *pSetCount,
                                         uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                         zet_typed_value_t *pMetricValues) override;
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override;
    ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) override;
    static std::unique_ptr<MultiDeviceIpSamplingMetricGroupImp> create(std::vector<IpSamplingMetricGroupImp *> &subDeviceMetricGroup);

  private:
    void closeSubDeviceStreamers(std::vector<IpSamplingMetricStreamerImp *> &subDeviceStreamers);
    std::vector<IpSamplingMetricGroupImp *> subDeviceMetricGroup = {};
};

struct IpSamplingMetricImp : public Metric {
    ~IpSamplingMetricImp() override = default;
    IpSamplingMetricImp(zet_metric_properties_t &properties);
    ze_result_t getProperties(zet_metric_properties_t *pProperties) override;

  private:
    zet_metric_properties_t properties;
};

template <>
IpSamplingMetricSourceImp &MetricDeviceContext::getMetricSource<IpSamplingMetricSourceImp>() const;

} // namespace L0
