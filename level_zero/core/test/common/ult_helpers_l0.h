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
            device->getNEODevice()->resetUsmAllocationPool(nullptr);
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