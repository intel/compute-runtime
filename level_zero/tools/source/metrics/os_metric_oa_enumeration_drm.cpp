/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/linux/os_metric_oa_enumeration_imp_linux.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

namespace L0 {

bool MetricEnumeration::getAdapterId(uint32_t &major, uint32_t &minor) {
    auto &device = metricSource.getMetricDeviceContext().getDevice();
    return getDrmAdapterId(major, minor, device);
}

MetricsDiscovery::IAdapter_1_13 *MetricEnumeration::getMetricsAdapter() {
    return getDrmMetricsAdapter(this);
}

std::unique_ptr<MetricOAOsInterface> MetricOAOsInterface::create(Device &device) {
    return std::make_unique<MetricOALinuxImp>(device);
}

} // namespace L0
