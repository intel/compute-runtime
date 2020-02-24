/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"

namespace NEO {

std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment) {
    if (ultHwConfig.forceOsAgnosticMemoryManager) {
        return std::make_unique<OsAgnosticMemoryManager>(executionEnvironment);
    }
    return std::make_unique<WddmMemoryManager>(executionEnvironment);
}
} // namespace NEO
