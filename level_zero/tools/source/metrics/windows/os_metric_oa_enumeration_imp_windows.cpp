/*
 * Copyright (C) 2020-2023 Intel Corporation
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
    for (uint32_t index = 0, count = getAdapterGroupParams(pAdapterGroup)->AdapterCount;
         index < count;
         ++index) {

        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index) == nullptr);
        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index)->GetParams() == nullptr);

        auto adapter = getAdapterFromAdapterGroup(pAdapterGroup, index);
        auto adapterParams = getAdapterParams(adapter);

        const bool validAdapterInfo = adapterParams->SystemId.Type == MetricsDiscovery::ADAPTER_ID_TYPE_LUID;
        const bool validAdapterMatch = (static_cast<uint32_t>(adapterParams->SystemId.Luid.HighPart) == major) &&
                                       (adapterParams->SystemId.Luid.LowPart == minor);

        if (validAdapterInfo && validAdapterMatch) {
            return adapter;
        }
    }

    return nullptr;
}

class MetricOAWindowsImp : public MetricOAOsInterface {
  public:
    MetricOAWindowsImp() = default;
    ~MetricOAWindowsImp() override = default;
    ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) override;
};

std::unique_ptr<MetricOAOsInterface> MetricOAOsInterface::create(Device &device) {
    return std::make_unique<MetricOAWindowsImp>();
}

ze_result_t MetricOAWindowsImp::getMetricsTimerResolution(uint64_t &timerResolution) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
