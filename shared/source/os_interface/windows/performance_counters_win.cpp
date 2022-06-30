/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_counters_win.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_time_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {
/////////////////////////////////////////////////////
// PerformanceCounters::create
/////////////////////////////////////////////////////
std::unique_ptr<PerformanceCounters> PerformanceCounters::create(Device *device) {
    auto counter = std::make_unique<PerformanceCountersWin>();
    auto wddm = device->getOSTime()->getOSInterface()->getDriverModel()->as<Wddm>();
    auto gen = device->getHardwareInfo().platform.eRenderCoreFamily;
    auto &hwHelper = HwHelper::get(gen);
    UNRECOVERABLE_IF(counter == nullptr);

    counter->clientData.Windows.Adapter = reinterpret_cast<void *>(static_cast<UINT_PTR>(wddm->getAdapter()));
    counter->clientData.Windows.Device = reinterpret_cast<void *>(static_cast<UINT_PTR>(wddm->getDeviceHandle()));
    counter->clientData.Windows.Escape = wddm->getEscapeHandle();
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

    // Oa configuration.
    if (oaConfiguration.IsValid()) {
        metricsLibrary->oaConfigurationDeactivate(oaConfiguration);
        metricsLibrary->oaConfigurationDelete(oaConfiguration);
        oaConfiguration.data = nullptr;
    }
}
} // namespace NEO
