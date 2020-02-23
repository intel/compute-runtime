/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_counters_win.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/os_time_win.h"

namespace NEO {
/////////////////////////////////////////////////////
// PerformanceCounters::create
/////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> PerformanceCounters::create(Device *device) {
    auto counter = std::make_unique<PerformanceCountersWin>();
    auto osInterface = device->getOSTime()->getOSInterface()->get();
    auto gen = device->getHardwareInfo().platform.eRenderCoreFamily;
    auto &hwHelper = HwHelper::get(gen);
    UNRECOVERABLE_IF(counter == nullptr);

    counter->clientData.Windows.Adapter = reinterpret_cast<void *>(static_cast<UINT_PTR>(osInterface->getAdapterHandle()));
    counter->clientData.Windows.Device = reinterpret_cast<void *>(static_cast<UINT_PTR>(osInterface->getDeviceHandle()));
    counter->clientData.Windows.Device = reinterpret_cast<void *>(static_cast<UINT_PTR>(osInterface->getDeviceHandle()));
    counter->clientData.Windows.Escape = osInterface->getEscapeHandle();
    counter->clientData.Windows.KmdInstrumentationEnabled = device->getHardwareInfo().capabilityTable.instrumentationEnabled;
    counter->contextData.ClientData = &counter->clientData;
    counter->clientType.Gen = static_cast<MetricsLibraryApi::ClientGen>(hwHelper.getMetricsLibraryGenId());

    return counter;
}

//////////////////////////////////////////////////////
// PerformanceCountersWin::enableCountersConfiguration
//////////////////////////////////////////////////////
bool PerformanceCountersWin::enableCountersConfiguration() {
    // Release previous counters configuration so the user
    // can change configuration between kernels.
    releaseCountersConfiguration();

    // Create mmio user configuration.
    if (!metricsLibrary->userConfigurationCreate(
            context,
            userConfiguration)) {
        DEBUG_BREAK_IF(true);
        return false;
    }

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
// PerformanceCountersWin::releaseCountersConfiguration
//////////////////////////////////////////////////////
void PerformanceCountersWin::releaseCountersConfiguration() {
    // Mmio user configuration.
    if (userConfiguration.IsValid()) {
        metricsLibrary->userConfigurationDelete(userConfiguration);
        userConfiguration.data = nullptr;
    }

    // Oa configuration.
    if (oaConfiguration.IsValid()) {
        metricsLibrary->oaConfigurationDeactivate(oaConfiguration);
        metricsLibrary->oaConfigurationDelete(oaConfiguration);
        oaConfiguration.data = nullptr;
    }
}
} // namespace NEO
