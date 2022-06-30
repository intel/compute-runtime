/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_manager.h"

namespace NEO {
std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment, DriverModelType driverModel) {
    return DrmMemoryManager::create(executionEnvironment);
}

size_t getSizeToReserve() {
    return (maxNBitValue(47) + 1) / 4;
}
} // namespace NEO
