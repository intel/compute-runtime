/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include <sys/stat.h>
#include <sys/sysmacros.h>

namespace L0 {

void MetricEnumeration::getMetricsDiscoveryFilename(std::vector<const char *> &names) const {
    names.clear();
    names.push_back("libigdmd.so.1");
    names.push_back("libmd.so.1");
}

bool MetricEnumeration::getAdapterId(uint32_t &adapterMajor, uint32_t &adapterMinor) {

    auto &device = metricSource.getMetricDeviceContext().getDevice();
    auto &osInterface = device.getOsInterface();
    auto drm = osInterface.getDriverModel()->as<NEO::Drm>();
    auto drmFile = drm->getFileDescriptor();
    struct stat drmStat = {};

    int32_t result = NEO::SysCalls::fstat(drmFile, &drmStat);

    adapterMajor = major(drmStat.st_rdev);
    adapterMinor = minor(drmStat.st_rdev);

    return result == 0;
}

MetricsDiscovery::IAdapter_1_9 *MetricEnumeration::getMetricsAdapter() {

    UNRECOVERABLE_IF(pAdapterGroup == nullptr);

    // Obtain drm minor / major version.
    uint32_t drmMajor = 0;
    uint32_t drmMinor = 0;

    UNRECOVERABLE_IF(getAdapterId(drmMajor, drmMinor) == false);

    // Driver drm major/minor version.
    const int32_t drmNodePrimary = 0; // From xf86drm.h
    const int32_t drmNodeRender = 2;  // From xf86drm.h
    const int32_t drmMaxDevices = 64; // From drm_drv.c#110

    const int32_t drmMinorRender = drmMinor - (drmNodeRender * drmMaxDevices);
    const int32_t drmMinorPrimary = drmMinor - (drmNodePrimary * drmMaxDevices);

    // Enumerate metrics discovery adapters.
    for (uint32_t index = 0, count = pAdapterGroup->GetParams()->AdapterCount;
         index < count;
         ++index) {

        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index) == nullptr);
        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index)->GetParams() == nullptr);

        auto adapter = pAdapterGroup->GetAdapter(index);
        auto adapterParams = adapter->GetParams();

        const bool validAdapterType = adapterParams->SystemId.Type == MetricsDiscovery::ADAPTER_ID_TYPE_MAJOR_MINOR;
        const bool validAdapterMajor = adapterParams->SystemId.MajorMinor.Major == static_cast<int32_t>(drmMajor);
        const bool validAdapterMinor = (adapterParams->SystemId.MajorMinor.Minor == drmMinorRender) ||
                                       (adapterParams->SystemId.MajorMinor.Minor == drmMinorPrimary);

        if (validAdapterType && validAdapterMajor && validAdapterMinor) {
            return adapter;
        }
    }

    return nullptr;
}

} // namespace L0
