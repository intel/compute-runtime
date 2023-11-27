/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_with_aub_dump.h"
#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

std::unique_ptr<DrmMemoryOperationsHandler> DrmMemoryOperationsHandler::create(Drm &drm, uint32_t rootDeviceIndex, bool withAubDump) {
    bool useVmBind = drm.isVmBindAvailable();
    auto rootDeviceEnv = drm.getRootDeviceEnvironment().executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get();

    if (useVmBind) {
        if (withAubDump) {
            return std::make_unique<DrmMemoryOperationsHandlerWithAubDump<DrmMemoryOperationsHandlerBind>>(*rootDeviceEnv, rootDeviceIndex);
        } else {
            return std::make_unique<DrmMemoryOperationsHandlerBind>(drm.getRootDeviceEnvironment(), rootDeviceIndex);
        }
    }

    if (withAubDump) {
        return std::make_unique<DrmMemoryOperationsHandlerWithAubDump<DrmMemoryOperationsHandlerDefault>>(*rootDeviceEnv, rootDeviceIndex);
    } else {
        return std::make_unique<DrmMemoryOperationsHandlerDefault>(rootDeviceIndex);
    }
}

} // namespace NEO
