/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

using namespace MetricsLibraryApi;

namespace L0 {

const char *MetricsLibrary::getFilename() { return "libigdml.so.1"; }

bool MetricsLibrary::getContextData(Device &device, ContextCreateData_1_0 &contextData) {

    auto &osInterface = device.getOsInterface();
    auto drm = osInterface.getDriverModel()->as<NEO::Drm>();
    auto drmFileDescriptor = drm->getFileDescriptor();
    auto &osData = contextData.ClientData->Linux;

    osData.Adapter->Type = LinuxAdapterType::DrmFileDescriptor;
    osData.Adapter->DrmFileDescriptor = drmFileDescriptor;

    return drmFileDescriptor != -1;
}

bool MetricsLibrary::activateConfiguration(const ConfigurationHandle_1_0 configurationHandle) {

    ConfigurationActivateData_1_0 activateData = {};
    activateData.Type = GpuConfigurationActivationType::Tbs;

    const bool validMetricsLibrary = isInitialized();
    const bool validConfiguration = configurationHandle.IsValid();
    const bool result =
        validMetricsLibrary && validConfiguration &&
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
    // Linux does not support configuration cache.
    // Any previous configuration should be deleted.
    deleteAllConfigurations();

    // Cache only a single configuration.
    configurations[metricGroup] = configurationHandle;
}
} // namespace L0
