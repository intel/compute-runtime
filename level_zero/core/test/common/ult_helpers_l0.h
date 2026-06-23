/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

namespace L0 {
namespace ult {
struct L0UltHelper {
    static void cleanupUsmAllocPoolsAndReuse(DriverHandle *driverHandle) {
        driverHandle->usmHostMemAllocPoolFacade.cleanup();
        for (auto device : driverHandle->devices) {
            device->getNEODevice()->cleanupUsmAllocationPool();
        }
        driverHandle->svmAllocsManager->cleanupUSMAllocCaches();
    }

    static void initUsmAllocPools(DriverHandle *driverHandle) {
        driverHandle->initHostUsmAllocPool(driverHandle->numDevices > 1);
        for (auto device : driverHandle->devices) {
            driverHandle->initDeviceUsmAllocPool(*device->getNEODevice(), driverHandle->numDevices > 1);
        }
    }
};
} // namespace ult
} // namespace L0
