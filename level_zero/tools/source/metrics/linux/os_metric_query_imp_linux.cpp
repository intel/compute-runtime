/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric_query_imp.h"

using namespace MetricsLibraryApi;

namespace L0 {

const char *MetricsLibrary::getFilename() { return "libigdml64.so"; }

bool MetricsLibrary::getContextData(Device &device, ContextCreateData_1_0 &contextData) {
    return true;
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

} // namespace L0
