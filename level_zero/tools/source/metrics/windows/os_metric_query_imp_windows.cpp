/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_interface.h"

#include "level_zero/core/source/device.h"
#include "level_zero/tools/source/metrics/metric_query_imp.h"

#if defined(_WIN64)
#define METRICS_LIBRARY_NAME "igdml64.dll"
#elif defined(_WIN32)
#define METRICS_LIBRARY_NAME "igdml32.dll"
#else
#error "Unsupported OS"
#endif

using namespace MetricsLibraryApi;

namespace L0 {

///////////////////////////////////////////////////////////////////////////////
/// @brief Returns metrics library file name.
const char *MetricsLibrary::getFilename() { return METRICS_LIBRARY_NAME; }

///////////////////////////////////////////////////////////////////////////////
/// @brief Returns os specific data required to initialize metrics library.
bool MetricsLibrary::getContextData(Device &device, ContextCreateData_1_0 &contextData) {

    auto osInterface = device.getOsInterface().get();
    auto &osData = contextData.ClientData->Windows;

    // Copy escape data (adapter/device/escape function).
    osData.KmdInstrumentationEnabled = true;
    osData.Device = reinterpret_cast<void *>(static_cast<UINT_PTR>(osInterface->getDeviceHandle()));
    osData.Escape = osInterface->getEscapeHandle();
    osData.Adapter =
        reinterpret_cast<void *>(static_cast<UINT_PTR>(osInterface->getAdapterHandle()));

    return osData.Device && osData.Escape && osData.Adapter;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Writes metric group configuration to gpu.
bool MetricsLibrary::activateConfiguration(const ConfigurationHandle_1_0 configurationHandle) {

    ConfigurationActivateData_1_0 activateData = {};
    activateData.Type = GpuConfigurationActivationType::EscapeCode;

    const bool validMetricsLibrary = isInitialized();
    const bool validConfiguration = configurationHandle.IsValid();
    const bool result = validMetricsLibrary && validConfiguration &&
                        (api.ConfigurationActivate(configurationHandle, &activateData) == StatusCode::Success);

    UNRECOVERABLE_IF(!result);
    return result;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Disables activated metric group configuration.
bool MetricsLibrary::deactivateConfiguration(const ConfigurationHandle_1_0 configurationHandle) {

    const bool validMetricsLibrary = isInitialized();
    const bool validConfiguration = configurationHandle.IsValid();
    const bool result = validMetricsLibrary && validConfiguration &&
                        (api.ConfigurationDeactivate(configurationHandle) == StatusCode::Success);

    UNRECOVERABLE_IF(!result);
    return result;
}

} // namespace L0
