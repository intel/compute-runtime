/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"
#include <level_zero/zet_api.h>

namespace L0 {
constexpr uint32_t ipSamplinMetricCount = 10u;
constexpr uint32_t ipSamplinDomainId = 100u;

std::unique_ptr<IpSamplingMetricSourceImp> IpSamplingMetricSourceImp::create(const MetricDeviceContext &metricDeviceContext) {
    return std::unique_ptr<IpSamplingMetricSourceImp>(new (std::nothrow) IpSamplingMetricSourceImp(metricDeviceContext));
}

IpSamplingMetricSourceImp::IpSamplingMetricSourceImp(const MetricDeviceContext &metricDeviceContext) : metricDeviceContext(metricDeviceContext) {
    metricOsInterface = MetricIpSamplingOsInterface::create(metricDeviceContext.getDevice());
}

void IpSamplingMetricSourceImp::enable() {
    isEnabled = metricOsInterface->isDependencyAvailable();
}

bool IpSamplingMetricSourceImp::isAvailable() {
    return isEnabled;
}

void IpSamplingMetricSourceImp::cacheMetricGroup() {

    std::vector<IpSamplingMetricImp> metrics = {};
    metrics.reserve(ipSamplinMetricCount);

    zet_metric_properties_t metricProperties = {};

    metricProperties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
    metricProperties.pNext = nullptr;
    strcpy_s(metricProperties.component, ZET_MAX_METRIC_COMPONENT, "XVE");
    metricProperties.tierNumber = 4;
    metricProperties.resultType = ZET_VALUE_TYPE_UINT64;

    // Preparing properties for IP seperately because of unique values
    strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, "IP");
    strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, "IP address");
    metricProperties.metricType = ZET_METRIC_TYPE_IP_EXP;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Address");
    metrics.push_back(IpSamplingMetricImp(metricProperties));

    std::vector<std::pair<const char *, const char *>> metricPropertiesList = {
        {"Active", "Active cycles"},
        {"ControlStall", "Stall on control"},
        {"PipeStall", "Stall on pipe"},
        {"SendStall", "Stall on send"},
        {"DistStall", "Stall on distance"},
        {"SbidStall", "Stall on scoreboard"},
        {"SyncStall", "Stall on sync"},
        {"InstrFetchStall", "Stall on instruction fetch"},
        {"OtherStall", "Stall on other condition"},
    };

    // Preparing properties for others because of common values
    metricProperties.metricType = ZET_METRIC_TYPE_EVENT;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Events");

    for (auto &property : metricPropertiesList) {
        strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, property.first);
        strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, property.second);
        metrics.push_back(IpSamplingMetricImp(metricProperties));
    }

    cachedMetricGroup = IpSamplingMetricGroupImp::create(metrics);
    DEBUG_BREAK_IF(cachedMetricGroup == nullptr);
}

ze_result_t IpSamplingMetricSourceImp::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {

    if (!isEnabled) {
        *pCount = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }

    if (cachedMetricGroup == nullptr) {
        cacheMetricGroup();
    }

    DEBUG_BREAK_IF(phMetricGroups == nullptr);
    phMetricGroups[0] = cachedMetricGroup->toHandle();
    *pCount = 1;

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void IpSamplingMetricSourceImp::setMetricOsInterface(std::unique_ptr<MetricIpSamplingOsInterface> &metricOsInterface) {
    this->metricOsInterface = std::move(metricOsInterface);
}

IpSamplingMetricGroupImp::IpSamplingMetricGroupImp(std::vector<IpSamplingMetricImp> &metrics) {
    this->metrics.reserve(metrics.size());
    for (const auto &metric : metrics) {
        this->metrics.push_back(std::make_unique<IpSamplingMetricImp>(metric));
    }

    properties.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    properties.pNext = nullptr;
    strcpy_s(properties.name, ZET_MAX_METRIC_GROUP_NAME, "EuStallSampling");
    strcpy_s(properties.description, ZET_MAX_METRIC_GROUP_DESCRIPTION, "EU stall sampling");
    properties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;
    properties.domain = ipSamplinDomainId;
    properties.metricCount = ipSamplinMetricCount;
}

ze_result_t IpSamplingMetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {
    *pProperties = properties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {

    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(metrics.size());
        return ZE_RESULT_SUCCESS;
    }
    // User is expected to allocate space.
    DEBUG_BREAK_IF(phMetrics == nullptr);

    *pCount = std::min(*pCount, static_cast<uint32_t>(metrics.size()));

    for (uint32_t i = 0; i < *pCount; i++) {
        phMetrics[i] = metrics[i]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                            const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                            zet_typed_value_t *pMetricValues) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t IpSamplingMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                               const uint8_t *pRawData, uint32_t *pSetCount,
                                                               uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                               zet_typed_value_t *pMetricValues) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool IpSamplingMetricGroupImp::activate() {
    // There is no hardware specific activation, since metric collection starts in streamer open
    return true;
}

bool IpSamplingMetricGroupImp::deactivate() {
    return true;
}
zet_metric_group_handle_t IpSamplingMetricGroupImp::getMetricGroupForSubDevice(const uint32_t subDeviceIndex) {
    return toHandle();
}

ze_result_t IpSamplingMetricGroupImp::metricQueryPoolCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::unique_ptr<IpSamplingMetricGroupImp> IpSamplingMetricGroupImp::create(std::vector<IpSamplingMetricImp> &ipSamplingMetrics) {
    return std::unique_ptr<IpSamplingMetricGroupImp>(new (std::nothrow) IpSamplingMetricGroupImp(ipSamplingMetrics));
}

IpSamplingMetricImp::IpSamplingMetricImp(zet_metric_properties_t &properties) : properties(properties) {
}

ze_result_t IpSamplingMetricImp::getProperties(zet_metric_properties_t *pProperties) {
    *pProperties = properties;
    return ZE_RESULT_SUCCESS;
}

template <>
IpSamplingMetricSourceImp &MetricDeviceContext::getMetricSource<IpSamplingMetricSourceImp>() const {
    return static_cast<IpSamplingMetricSourceImp &>(*metricSources.at(MetricSource::SourceType::IpSampling));
}

} // namespace L0
