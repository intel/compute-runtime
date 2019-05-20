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
    data.Type = GpuConfigurationActivationType::EscapeCode;

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
    ConfigurationCreateData_1_0 data = {};
    data.HandleContext = context;
    data.Type = ObjectType::ConfigurationHwCountersUser;

    return api->functions.ConfigurationCreate(
               &data,
               &handle) == StatusCode::Success;
}

//////////////////////////////////////////////////////
// FUNCTION: MetricsLibrary::userConfigurationDelete
//////////////////////////////////////////////////////
bool MetricsLibrary::userConfigurationDelete(
    const ConfigurationHandle_1_0 &handle) {
    return api->functions.ConfigurationDelete(handle) == StatusCode::Success;
}
} // namespace NEO