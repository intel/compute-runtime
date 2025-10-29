/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {
namespace ult {
struct L0UltHelper {
    static void cleanupUsmAllocPoolsAndReuse(DriverHandleImp *driverHandle) {
        if (driverHandle->usmHostMemAllocPool) {
            driverHandle->usmHostMemAllocPool->cleanup();
            driverHandle->usmHostMemAllocPool.reset(nullptr);
        }
        if (driverHandle->usmHostMemAllocPoolManager) {
            driverHandle->usmHostMemAllocPoolManager->cleanup();
            driverHandle->usmHostMemAllocPoolManager.reset(nullptr);
        }
        for (auto device : driverHandle->devices) {
            device->getNEODevice()->cleanupUsmAllocationPool();
        }
        driverHandle->svmAllocsManager->cleanupUSMAllocCaches();
    }

    static void initUsmAllocPools(DriverHandleImp *driverHandle) {
        driverHandle->initHostUsmAllocPool();
        for (auto device : driverHandle->devices) {
            driverHandle->initDeviceUsmAllocPool(*device->getNEODevice(), driverHandle->numDevices > 1);
        }
    }
};
} // namespace ult
} // namespace L0