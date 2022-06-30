/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/helpers/ult_hw_config.h"

namespace NEO {

std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment, DriverModelType driverModel) {
    if (ultHwConfig.forceOsAgnosticMemoryManager) {
        return std::make_unique<OsAgnosticMemoryManager>(executionEnvironment);
    }
    return std::make_unique<WddmMemoryManager>(executionEnvironment);
}
} // namespace NEO
