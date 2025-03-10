/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"

namespace L0 {
namespace ult {

class MockMetricIpSamplingSource : public IpSamplingMetricSourceImp {
  public:
    ze_result_t createMetricGroupsFromMetricsReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    ze_result_t metricProgrammableGetReturn = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    MockMetricIpSamplingSource(const MetricDeviceContext &metricDeviceContext) : IpSamplingMetricSourceImp(metricDeviceContext) {}
    ~MockMetricIpSamplingSource() override = default;

    ze_result_t metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) override {
        return metricProgrammableGetReturn;
    }

    bool isAvailable() override { return true; }
    void enable() override {}
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) override {
        *pCount = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t activateMetricGroupsAlreadyDeferred() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t activateMetricGroupsPreferDeferred(const uint32_t count, zet_metric_group_handle_t *phMetricGroups) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
        return false;
    }
    ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                          uint32_t *pConcurrentGroupCount,
                                          uint32_t *pCountPerConcurrentGroup) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t createMetricGroupsFromMetrics(std::vector<zet_metric_handle_t> &metricList,
                                              const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                              const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                              uint32_t *maxMetricGroupCount,
                                              std::vector<zet_metric_group_handle_t> &metricGroupList) override {
        return createMetricGroupsFromMetricsReturn;
    }

    ze_result_t handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup,
                                                    zet_metric_group_properties_t *pBaseProperties,
                                                    void *pNext) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
};

class MockMetricDeviceContextIpSampling : public MetricDeviceContext {
  public:
    MockMetricDeviceContextIpSampling(Device &device) : MetricDeviceContext(device) {}

    void setMetricIpSamplingSource(MockMetricIpSamplingSource *metricSource) {
        metricSources[MetricSource::metricSourceTypeIpSampling] = std::unique_ptr<MockMetricIpSamplingSource>(metricSource);
    }
};

} // namespace ult
} // namespace L0