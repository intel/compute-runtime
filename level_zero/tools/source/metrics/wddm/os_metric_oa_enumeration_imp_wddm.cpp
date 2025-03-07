/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/wddm/os_metric_oa_enumeration_imp_wddm.h"

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

namespace L0 {

bool getWddmAdapterId(uint32_t &major, uint32_t &minor, Device &device) {
    auto wddm = device.getOsInterface()->getDriverModel()->as<NEO::Wddm>();
    auto luid = wddm->getAdapterLuid();

    major = luid.HighPart;
    minor = luid.LowPart;

    return true;
}

MetricsDiscovery::IAdapter_1_13 *getWddmMetricsAdapter(MetricEnumeration *metricEnumeration) {
    uint32_t major = 0;
    uint32_t minor = 0;

    auto pAdapterGroup = metricEnumeration->getMdapiAdapterGroup();
    auto &device = metricEnumeration->getMetricSource().getMetricDeviceContext().getDevice();

    UNRECOVERABLE_IF(pAdapterGroup == nullptr);
    UNRECOVERABLE_IF(getWddmAdapterId(major, minor, device) == false);

    // Enumerate metrics discovery adapters.
    for (uint32_t index = 0, count = metricEnumeration->getAdapterGroupParams(pAdapterGroup)->AdapterCount;
         index < count;
         ++index) {

        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index) == nullptr);
        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index)->GetParams() == nullptr);

        auto adapter = metricEnumeration->getAdapterFromAdapterGroup(pAdapterGroup, index);
        auto adapterParams = metricEnumeration->getAdapterParams(adapter);

        const bool validAdapterInfo = adapterParams->SystemId.Type == MetricsDiscovery::ADAPTER_ID_TYPE_LUID;
        const bool validAdapterMatch = (static_cast<uint32_t>(adapterParams->SystemId.Luid.HighPart) == major) &&
                                       (adapterParams->SystemId.Luid.LowPart == minor);

        if (validAdapterInfo && validAdapterMatch) {
            return adapter;
        }
    }

    return nullptr;
}
} // namespace L0
