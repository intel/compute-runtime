/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include "level_zero/include/zet_intel_gpu_metric.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_programmable_imp.h"

namespace L0 {

ze_result_t ZE_APICALL
zetIntelDeviceGetConcurrentMetricGroupsExp(
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    uint32_t *pConcurrentGroupCount,
    uint32_t *pCountPerConcurrentGroup) {

    auto device = Device::fromHandle(toInternalType(hDevice));
    return static_cast<MetricDeviceContext &>(device->getMetricDeviceContext()).getConcurrentMetricGroups(metricGroupCount, phMetricGroups, pConcurrentGroupCount, pCountPerConcurrentGroup);
}

ze_result_t ZE_APICALL
zetIntelDeviceCreateMetricGroupsFromMetricsExp(
    zet_device_handle_t hDevice,
    uint32_t metricCount,
    zet_metric_handle_t *phMetrics,
    const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
    const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
    uint32_t *pMetricGroupCount,
    zet_metric_group_handle_t *phMetricGroups) {

    hDevice = toInternalType(hDevice);
    auto device = Device::fromHandle(hDevice);
    return device->getMetricDeviceContext().createMetricGroupsFromMetrics(metricCount, phMetrics, metricGroupNamePrefix, description, pMetricGroupCount, phMetricGroups);
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL
zetIntelDeviceGetConcurrentMetricGroupsExp(
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    uint32_t *pConcurrentGroupCount,
    uint32_t *pCountPerConcurrentGroup) {
    return L0::zetIntelDeviceGetConcurrentMetricGroupsExp(
        hDevice, metricGroupCount, phMetricGroups,
        pConcurrentGroupCount, pCountPerConcurrentGroup);
}

ze_result_t ZE_APICALL
zetIntelDeviceCreateMetricGroupsFromMetricsExp(
    zet_device_handle_t hDevice,
    uint32_t metricCount,
    zet_metric_handle_t *phMetrics,
    const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
    const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
    uint32_t *metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups) {

    return L0::zetIntelDeviceCreateMetricGroupsFromMetricsExp(
        hDevice, metricCount, phMetrics, metricGroupNamePrefix, description, metricGroupCount, phMetricGroups);
}

} // extern "C"
