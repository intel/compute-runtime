/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_counters_linux.h"

#include "shared/source/device/device.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
////////////////////////////////////////////////////
// PerformanceCounters::create
////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> PerformanceCounters::create(Device *device) {
    auto counter = std::make_unique<PerformanceCountersLinux>();
    auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    UNRECOVERABLE_IF(counter == nullptr);

    if (!device->isSubDevice()) {

        // Root device.
        counter->subDevice.Enabled = false;
        counter->subDeviceIndex.Index = 0;
        counter->subDeviceCount.Count = std::max(device->getNumSubDevices(), 1u);
    } else {

        // Sub device.
        counter->subDevice.Enabled = true;
        counter->subDeviceIndex.Index = static_cast<NEO::SubDevice *>(device)->getSubDeviceIndex();
        counter->subDeviceCount.Count = std::max(device->getRootDevice()->getNumSubDevices(), 1u);
    }

    // Adapter data.
    counter->adapter.Type = LinuxAdapterType::DrmFileDescriptor;
    counter->adapter.DrmFileDescriptor = drm->getFileDescriptor();
    counter->clientData.Linux.Adapter = &(counter->adapter);
    counter->metricsLibrary->api->callbacks.CommandBufferFlush = &PerformanceCounters::flushCommandBufferCallback;
    counter->clientData.Handle = reinterpret_cast<void *>(device);

    // Gen data.
    counter->clientType.Gen = static_cast<MetricsLibraryApi::ClientGen>(gfxCoreHelper.getMetricsLibraryGenId());

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
