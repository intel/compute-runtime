/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"

#include <algorithm>

namespace NEO {

void ExecutionEnvironment::sortNeoDevicesDRM() {
    std::vector<uint32_t> presortIndex;
    for (uint32_t i = 0; i < rootDeviceEnvironments.size(); i++) {
        NEO::DrmMemoryOperationsHandler *drm = static_cast<DrmMemoryOperationsHandler *>(rootDeviceEnvironments[i]->memoryOperationsInterface.get());
        presortIndex.push_back(drm->getRootDeviceIndex());
    }

    std::sort(rootDeviceEnvironments.begin(), rootDeviceEnvironments.end(), comparePciIdBusNumber);

    for (uint32_t i = 0; i < rootDeviceEnvironments.size(); i++) {
        NEO::DrmMemoryOperationsHandler *drm = static_cast<DrmMemoryOperationsHandler *>(rootDeviceEnvironments[i]->memoryOperationsInterface.get());
        if (drm->getRootDeviceIndex() != presortIndex[i]) {
            drm->setRootDeviceIndex(presortIndex[i]);
        }
    }
}

} // namespace NEO
