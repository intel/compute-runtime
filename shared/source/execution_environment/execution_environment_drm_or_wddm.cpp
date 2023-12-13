/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

void ExecutionEnvironment::adjustRootDeviceEnvironments() {
    if (!rootDeviceEnvironments.empty() && rootDeviceEnvironments[0]->osInterface->getDriverModel()->getDriverModelType() == DriverModelType::drm) {
        for (auto rootDeviceIndex = 0u; rootDeviceIndex < rootDeviceEnvironments.size(); rootDeviceIndex++) {
            auto drmMemoryOperationsHandler = static_cast<DrmMemoryOperationsHandler *>(rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
            drmMemoryOperationsHandler->setRootDeviceIndex(rootDeviceIndex);
        }
    }
}

} // namespace NEO
