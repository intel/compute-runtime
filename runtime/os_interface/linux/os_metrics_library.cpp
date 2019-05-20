/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/metrics_library.h"

namespace NEO {
//////////////////////////////////////////////////////
// FUNCTION: MetricsLibrary::oaConfigurationActivate
//////////////////////////////////////////////////////
bool MetricsLibrary::oaConfigurationActivate(
    const ConfigurationHandle_1_0 &handle) {
    ConfigurationActivateData_1_0 data = {};
    data.Type = GpuConfigurationActivationType::Tbs;

    return api->functions.ConfigurationActivate(
               handle,
               &data) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// FUNCTION: MetricsLibrary::oaConfigurationDeactivate
//////////////////////////////////////////////////////
bool MetricsLibrary::oaConfigurationDeactivate(
    const ConfigurationHandle_1_0 &handle) {
    return api->functions.ConfigurationDeactivate(
               handle) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// FUNCTION: MetricsLibrary::userConfigurationCreate
//////////////////////////////////////////////////////
bool MetricsLibrary::userConfigurationCreate(
    const ContextHandle_1_0 &context,
    ConfigurationHandle_1_0 &handle) {
    // Not supported on Linux.
    return true;
}

//////////////////////////////////////////////////////
// FUNCTION: MetricsLibrary::userConfigurationDelete
//////////////////////////////////////////////////////
bool MetricsLibrary::userConfigurationDelete(
    const ConfigurationHandle_1_0 &handle) {
    // Not supported on Linux.
    return true;
}
} // namespace NEO