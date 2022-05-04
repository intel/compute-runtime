/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

void ExecutionEnvironment::sortNeoDevices() {

    if (rootDeviceEnvironments[0]->osInterface->getDriverModel()->getDriverModelType() == DriverModelType::DRM) {
        return ExecutionEnvironment::sortNeoDevicesDRM();
    } else {
        return ExecutionEnvironment::sortNeoDevicesWDDM();
    }
}

} // namespace NEO