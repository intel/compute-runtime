/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/linux/os_metric_oa_enumeration_imp_linux.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

#include <sys/stat.h>
#include <sys/sysmacros.h>

namespace L0 {

void MetricEnumeration::getMetricsDiscoveryFilename(std::vector<const char *> &names) const {
    names.clear();
    names.push_back("libigdmd.so.1");
    names.push_back("libmd.so.1");
}

bool getDrmAdapterId(uint32_t &adapterMajor, uint32_t &adapterMinor, Device &device) {
    auto &osInterface = *device.getOsInterface();
    auto drm = osInterface.getDriverModel()->as<NEO::Drm>();
    auto drmFile = drm->getFileDescriptor();
    struct stat drmStat = {};

    int32_t result = NEO::SysCalls::fstat(drmFile, &drmStat);

    adapterMajor = major(drmStat.st_rdev);
    adapterMinor = minor(drmStat.st_rdev);

    return result == 0;
}

MetricsDiscovery::IAdapter_1_13 *getDrmMetricsAdapter(MetricEnumeration *metricEnumeration) {
    // Obtain drm minor / major version.
    uint32_t drmMajor = 0;
    uint32_t drmMinor = 0;

    auto pAdapterGroup = metricEnumeration->getMdapiAdapterGroup();
    auto &device = metricEnumeration->getMetricSource().getMetricDeviceContext().getDevice();

    UNRECOVERABLE_IF(pAdapterGroup == nullptr);
    UNRECOVERABLE_IF(getDrmAdapterId(drmMajor, drmMinor, device) == false);

    // Driver drm major/minor version.
    const int32_t drmNodePrimary = 0; // From xf86drm.h
    const int32_t drmNodeRender = 2;  // From xf86drm.h
    const int32_t drmMaxDevices = 64; // From drm_drv.c#110

    const int32_t drmMinorRender = drmMinor - (drmNodeRender * drmMaxDevices);
    const int32_t drmMinorPrimary = drmMinor - (drmNodePrimary * drmMaxDevices);

    // Enumerate metrics discovery adapters.
    for (uint32_t index = 0, count = metricEnumeration->getAdapterGroupParams(pAdapterGroup)->AdapterCount;
         index < count;
         ++index) {

        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index) == nullptr);
        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index)->GetParams() == nullptr);

        auto adapter = metricEnumeration->getAdapterFromAdapterGroup(pAdapterGroup, index);
        auto adapterParams = metricEnumeration->getAdapterParams(adapter);

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
