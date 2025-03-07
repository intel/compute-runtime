/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/linux/os_metric_oa_enumeration_imp_linux.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/wddm/os_metric_oa_enumeration_imp_wddm.h"

namespace L0 {

bool MetricEnumeration::getAdapterId(uint32_t &major, uint32_t &minor) {
    auto &device = metricSource.getMetricDeviceContext().getDevice();
    auto osInterface = device.getNEODevice()->getRootDeviceEnvironment().osInterface.get();

    if (osInterface && osInterface->getDriverModel()->getDriverModelType() == NEO::DriverModelType::wddm)
        return getWddmAdapterId(major, minor, device);
    else
        return getDrmAdapterId(major, minor, device);
}

MetricsDiscovery::IAdapter_1_13 *MetricEnumeration::getMetricsAdapter() {
    auto osInterface = metricSource.getMetricDeviceContext().getDevice().getNEODevice()->getRootDeviceEnvironment().osInterface.get();

    if (osInterface && osInterface->getDriverModel()->getDriverModelType() == NEO::DriverModelType::wddm)
        return getWddmMetricsAdapter(this);
    else
        return getDrmMetricsAdapter(this);
}
} // namespace L0
