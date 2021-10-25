/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/ult_hw_config.h"

namespace NEO {

std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment, DriverModelType driverModel) {
    if (ultHwConfig.forceOsAgnosticMemoryManager) {
        return std::make_unique<OsAgnosticMemoryManager>(executionEnvironment);
    }
    return DrmMemoryManager::create(executionEnvironment);
}
size_t getSizeToReserve() {
    // 4 x sizeof(Heap32) + 2 x sizeof(Standard/Standard64k)
    return (4 * 4 + 2 * 4) * GB;
}
} // namespace NEO
