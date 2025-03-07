/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/wddm/os_metric_oa_enumeration_imp_wddm.h"

namespace L0 {

bool MetricEnumeration::getAdapterId(uint32_t &major, uint32_t &minor) {
    auto &device = metricSource.getMetricDeviceContext().getDevice();
    return getWddmAdapterId(major, minor, device);
}

MetricsDiscovery::IAdapter_1_13 *MetricEnumeration::getMetricsAdapter() {
    return getWddmMetricsAdapter(this);
}

} // namespace L0
