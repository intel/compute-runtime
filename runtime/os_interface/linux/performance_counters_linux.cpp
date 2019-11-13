/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_counters_linux.h"

#include "core/helpers/hw_helper.h"
#include "runtime/device/device.h"

namespace NEO {
////////////////////////////////////////////////////
// PerformanceCounters::create
////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> PerformanceCounters::create(Device *device) {
    auto counter = std::make_unique<PerformanceCountersLinux>();
    auto gen = device->getHardwareInfo().platform.eRenderCoreFamily;
    auto &hwHelper = HwHelper::get(gen);
    UNRECOVERABLE_IF(counter == nullptr);

    counter->clientType.Gen = static_cast<MetricsLibraryApi::ClientGen>(hwHelper.getMetricsLibraryGenId());
    return counter;
}

//////////////////////////////////////////////////////
// PerformanceCountersLinux::enableCountersConfiguration
//////////////////////////////////////////////////////
bool PerformanceCountersLinux::enableCountersConfiguration() {
    // Release previous counters configuration so the user
    // can change configuration between kernels.
    releaseCountersConfiguration();

    // Create oa configuration.
    if (!metricsLibrary->oaConfigurationCreate(
            context,
            oaConfiguration)) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    // Enable oa configuration.
    if (!metricsLibrary->oaConfigurationActivate(
            oaConfiguration)) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////
// PerformanceCountersLinux::releaseCountersConfiguration
//////////////////////////////////////////////////////
void PerformanceCountersLinux::releaseCountersConfiguration() {
    // Oa configuration.
    if (oaConfiguration.IsValid()) {
        metricsLibrary->oaConfigurationDeactivate(oaConfiguration);
        metricsLibrary->oaConfigurationDelete(oaConfiguration);
        oaConfiguration.data = nullptr;
    }
}
} // namespace NEO
