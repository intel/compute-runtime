/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"

#if defined(_WIN64)
#define METRICS_LIBRARY_NAME "igdml64.dll"
#elif defined(_WIN32)
#define METRICS_LIBRARY_NAME "igdml32.dll"
#else
#error "Unsupported OS"
#endif

using namespace MetricsLibraryApi;

namespace L0 {

const char *MetricsLibrary::getFilename() { return METRICS_LIBRARY_NAME; }

bool MetricsLibrary::getContextData(Device &device, ContextCreateData_1_0 &contextData) {

    auto wddm = device.getOsInterface().getDriverModel()->as<NEO::Wddm>();
    auto &osData = contextData.ClientData->Windows;

    // Copy escape data (adapter/device/escape function).
    osData.KmdInstrumentationEnabled = true;
    osData.Device = reinterpret_cast<void *>(static_cast<UINT_PTR>(wddm->getDeviceHandle()));
    osData.Escape = wddm->getEscapeHandle();
    osData.Adapter =
        reinterpret_cast<void *>(static_cast<UINT_PTR>(wddm->getAdapter()));

    return osData.Device && osData.Escape && osData.Adapter;
}

bool MetricsLibrary::activateConfiguration(const ConfigurationHandle_1_0 configurationHandle) {

    ConfigurationActivateData_1_0 activateData = {};
    activateData.Type = GpuConfigurationActivationType::EscapeCode;

    const bool validMetricsLibrary = isInitialized();
    const bool validConfiguration = configurationHandle.IsValid();
    const bool result = validMetricsLibrary && validConfiguration &&
                        (api.ConfigurationActivate(configurationHandle, &activateData) == StatusCode::Success);

    DEBUG_BREAK_IF(!result);
    return result;
}

bool MetricsLibrary::deactivateConfiguration(const ConfigurationHandle_1_0 configurationHandle) {

    const bool validMetricsLibrary = isInitialized();
    const bool validConfiguration = configurationHandle.IsValid();
    const bool result = validMetricsLibrary && validConfiguration &&
                        (api.ConfigurationDeactivate(configurationHandle) == StatusCode::Success);

    DEBUG_BREAK_IF(!result);
    return result;
}

void MetricsLibrary::cacheConfiguration(zet_metric_group_handle_t metricGroup, ConfigurationHandle_1_0 configurationHandle) {
    configurations[metricGroup] = configurationHandle;
}
} // namespace L0
