/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_metric.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

ze_result_t ZE_APICALL
zexDeviceGetConcurrentMetricGroups(
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    uint32_t *pConcurrentGroupCount,
    uint32_t *pCountPerConcurrentGroup) {

    auto device = Device::fromHandle(toInternalType(hDevice));
    return static_cast<MetricDeviceContext &>(device->getMetricDeviceContext()).getConcurrentMetricGroups(metricGroupCount, phMetricGroups, pConcurrentGroupCount, pCountPerConcurrentGroup);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexDeviceGetConcurrentMetricGroups(
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    uint32_t *pConcurrentGroupCount,
    uint32_t *pCountPerConcurrentGroup) {
    return L0::zexDeviceGetConcurrentMetricGroups(
        hDevice, metricGroupCount, phMetricGroups,
        pConcurrentGroupCount, pCountPerConcurrentGroup);
}

} // extern "C"
