/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/io_functions.h"

#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"

#include <sys/stat.h>
#include <sys/sysmacros.h>

namespace L0 {

const char *MetricEnumeration::getMetricsDiscoveryFilename() { return "libmd.so.1"; }

bool MetricEnumeration::getAdapterId(uint32_t &adapterMajor, uint32_t &adapterMinor) {

    auto &device = metricContext.getDevice();
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

bool MetricEnumeration::isReportTriggerAvailable() {

    const char *perfStreamParanoidFile = "/proc/sys/dev/i915/perf_stream_paranoid";
    bool reportTriggerAvailable = false;

    FILE *fileDescriptor = NEO::IoFunctions::fopenPtr(perfStreamParanoidFile, "rb");
    if (fileDescriptor != nullptr) {
        char paranoidVal[2] = {0};
        size_t bytesRead = NEO::IoFunctions::freadPtr(&paranoidVal[0], 1, 1, fileDescriptor);
        NEO::IoFunctions::fclosePtr(fileDescriptor);

        if (bytesRead == 1) {
            reportTriggerAvailable = strncmp(paranoidVal, "0", 1) == 0;
        }
    }

    if (!reportTriggerAvailable) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "%s",
                              "INFO: Metrics Collection requires i915 paranoid mode to be disabled\n"
                              "INFO: set /proc/sys/dev/i915/perf_stream_paranoid to 0\n");
    }

    return reportTriggerAvailable;
}

} // namespace L0
