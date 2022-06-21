/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#if defined(_WIN64)
#define METRICS_DISCOVERY_NAME "igdmd64.dll"
#elif defined(_WIN32)
#define METRICS_DISCOVERY_NAME "igdmd32.dll"
#else
#error "Unsupported OS"
#endif

namespace L0 {

void MetricEnumeration::getMetricsDiscoveryFilename(std::vector<const char *> &names) const {
    names.clear();
    names.push_back(METRICS_DISCOVERY_NAME);
}

bool MetricEnumeration::getAdapterId(uint32_t &major, uint32_t &minor) {

    auto &device = metricSource.getMetricDeviceContext().getDevice();
    auto wddm = device.getOsInterface().getDriverModel()->as<NEO::Wddm>();
    auto luid = wddm->getAdapterLuid();

    major = luid.HighPart;
    minor = luid.LowPart;

    return true;
}

MetricsDiscovery::IAdapter_1_9 *MetricEnumeration::getMetricsAdapter() {

    uint32_t major = 0;
    uint32_t minor = 0;

    UNRECOVERABLE_IF(pAdapterGroup == nullptr);
    UNRECOVERABLE_IF(getAdapterId(major, minor) == false);

    // Enumerate metrics discovery adapters.
    for (uint32_t index = 0, count = pAdapterGroup->GetParams()->AdapterCount;
         index < count;
         ++index) {

        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index) == nullptr);
        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index)->GetParams() == nullptr);

        auto adapter = pAdapterGroup->GetAdapter(index);
        auto adapterParams = adapter->GetParams();

        const bool validAdapterInfo = adapterParams->SystemId.Type == MetricsDiscovery::ADAPTER_ID_TYPE_LUID;
        const bool validAdapterMatch = (adapterParams->SystemId.Luid.HighPart == major) &&
                                       (adapterParams->SystemId.Luid.LowPart == minor);

        if (validAdapterInfo && validAdapterMatch) {
            return adapter;
        }
    }

    return nullptr;
}

} // namespace L0
