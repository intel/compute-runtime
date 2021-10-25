/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"

#if defined(_WIN64)
#define METRICS_DISCOVERY_NAME "igdmd64.dll"
#elif defined(_WIN32)
#define METRICS_DISCOVERY_NAME "igdmd32.dll"
#else
#error "Unsupported OS"
#endif

namespace L0 {

const char *MetricEnumeration::getMetricsDiscoveryFilename() { return METRICS_DISCOVERY_NAME; }

bool MetricEnumeration::getAdapterId(uint32_t &major, uint32_t &minor) {

    auto &device = metricContext.getDevice();
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

bool MetricEnumeration::isReportTriggerAvailable() {
    return true;
}

} // namespace L0
