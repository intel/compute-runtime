/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

std::unique_ptr<DrmMemoryOperationsHandler> DrmMemoryOperationsHandler::create(Drm &drm, uint32_t rootDeviceIndex) {
    bool useVmBind = drm.isVmBindAvailable();

    if (useVmBind) {
        return std::make_unique<DrmMemoryOperationsHandlerBind>(drm.getRootDeviceEnvironment(), rootDeviceIndex);
    }

    return std::make_unique<DrmMemoryOperationsHandlerDefault>();
}

} // namespace NEO
