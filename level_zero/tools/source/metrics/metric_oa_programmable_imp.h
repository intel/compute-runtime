/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_multidevice_programmable.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

#include <vector>

namespace L0 {

struct OaMetricProgrammableImp : public MetricProgrammable {
    OaMetricProgrammableImp() = default;
    ~OaMetricProgrammableImp() override = default;
    ze_result_t getProperties(zet_metric_programmable_exp_properties_t *pProperties) override;
    ze_result_t getParamInfo(uint32_t *pParameterCount, zet_metric_programmable_param_info_exp_t *pParameterInfo) override;
    ze_result_t getParamValueInfo(uint32_t parameterOrdinal, uint32_t *pValueInfoCount,
                                  zet_metric_programmable_param_value_info_exp_t *pValueInfo) override;
    ze_result_t createMetric(zet_metric_programmable_param_value_exp_t *pParameterValues,
                             uint32_t parameterCount, const char name[ZET_MAX_METRIC_NAME],
                             const char description[ZET_MAX_METRIC_DESCRIPTION],
                             uint32_t *pMetricHandleCount, zet_metric_handle_t *phMetricHandles) override;
    void initialize(zet_metric_programmable_exp_properties_t &properties,
                    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                    MetricsDiscovery::IMetricPrototype_1_13 &prototype,
                    OaMetricSourceImp &metricSource);
    static MetricProgrammable *create(zet_metric_programmable_exp_properties_t &properties,
                                      MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                      MetricsDiscovery::IMetricPrototype_1_13 &prototype,
                                      MetricSource &metricSource);

  protected:
    zet_metric_programmable_exp_properties_t properties{};
    MetricsDiscovery::IConcurrentGroup_1_13 *pConcurrentGroup = nullptr;
    MetricsDiscovery::IMetricPrototype_1_13 *pPrototype = nullptr;
    MetricSource *metricSource = nullptr;

    ze_result_t copyMdapiValidValueToL0ValueInfo(zet_value_info_exp_t &l0Value, const MetricsDiscovery::TValidValue_1_13 &mdapiValue);
    ze_result_t copyL0ValueToMdapiTypedValue(MetricsDiscovery::TTypedValue_1_0 &mdapiValue, const zet_value_t &l0Value);
};

struct OaMetricFromProgrammable : OaMetricImp, MetricCreated {
    ~OaMetricFromProgrammable() override {}
    OaMetricFromProgrammable(MetricSource &metricSource) : OaMetricImp(metricSource) {}
    ze_result_t destroy() override;
    static Metric *create(MetricSource &metricSource, zet_metric_properties_t &properties,
                          MetricsDiscovery::IMetricPrototype_1_13 *pClonedPrototype,
                          uint32_t domain,
                          zet_metric_group_sampling_type_flags_t supportedSamplingTypes);
    MetricsDiscovery::IMetricPrototype_1_13 *pClonedPrototype = nullptr;
    uint32_t getDomain() { return domain; }
    zet_metric_group_sampling_type_flags_t getSupportedSamplingType() {
        return supportedSamplingTypes;
    }

  protected:
    uint32_t domain = 0;
    zet_metric_group_sampling_type_flags_t supportedSamplingTypes = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_FORCE_UINT32;
};

struct OaMetricGroupUserDefined : OaMetricGroupImp {
    ~OaMetricGroupUserDefined() override;
    bool activate() override;
    bool deactivate() override;
    static MetricGroup *create(zet_metric_group_properties_t &properties,
                               MetricsDiscovery::IMetricSet_1_13 &metricSet,
                               MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                               MetricSource &metricSource);
    ze_result_t addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) override;
    ze_result_t removeMetric(zet_metric_handle_t hMetric) override;
    ze_result_t close() override;
    ze_result_t destroy() override;

  protected:
    OaMetricGroupUserDefined(MetricSource &metricSource) : OaMetricGroupImp(metricSource) { isPredefined = false; }
    bool readyToActivate = false;
    bool isMetricSetOpened = false;
    bool isActivated = false;
    void removeMetrics(bool immutable, std::vector<Metric *> &removedMetricList);
    MetricsDiscovery::IMetricSet_1_13 *getMetricSet() {
        return static_cast<MetricsDiscovery::IMetricSet_1_13 *>(pReferenceMetricSet);
    }
};

struct OaMultiDeviceMetricGroupUserDefined : OaMetricGroupImp {

    OaMultiDeviceMetricGroupUserDefined(MetricSource &metricSource) : OaMetricGroupImp(metricSource) {
        isMultiDevice = true;
    }
    static OaMultiDeviceMetricGroupUserDefined *create(MetricSource &metricSource,
                                                       std::vector<MetricGroupImp *> &subDeviceMetricGroups,
                                                       std::vector<MultiDeviceMetricImp *> &multiDeviceMetrics);
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) override;
    ze_result_t removeMetric(zet_metric_handle_t hMetric) override;
    ze_result_t close() override;
    ze_result_t destroy() override;

  private:
    std::unique_ptr<MultiDeviceCreatedMetricGroupManager> createdMetricGroupManager = nullptr;
    std::vector<MultiDeviceMetricImp *> multiDeviceMetrics{};
};

} // namespace L0
