/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment, DriverModelType driverModel) {
    if (driverModel == DriverModelType::DRM) {
        return DrmMemoryManager::create(executionEnvironment);
    } else {
        return std::make_unique<WddmMemoryManager>(executionEnvironment);
    }
}
} // namespace NEO
