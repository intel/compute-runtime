/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment) {
    return std::make_unique<WddmMemoryManager>(executionEnvironment);
}
} // namespace NEO
