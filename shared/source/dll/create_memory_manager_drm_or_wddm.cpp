/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment, DriverModelType driverModel) {
    if (driverModel == DriverModelType::drm) {
        return DrmMemoryManager::create(executionEnvironment);
    } else {
        return std::make_unique<WddmMemoryManager>(executionEnvironment);
    }
}
size_t getSizeToReserve() {
    return (maxNBitValue(47) + 1) / 4;
}
} // namespace NEO
