/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"

namespace OCLRT {
std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(bool enable64KBpages, bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) {
    return std::make_unique<WddmMemoryManager>(enable64KBpages, enableLocalMemory, executionEnvironment.osInterface->get()->getWddm(), executionEnvironment);
}
} // namespace OCLRT
