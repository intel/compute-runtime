/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/os_interface.h"

namespace OCLRT {
std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(bool enable64KBpages, bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) {
    return std::make_unique<DrmMemoryManager>(executionEnvironment.osInterface->get()->getDrm(),
                                              gemCloseWorkerMode::gemCloseWorkerActive,
                                              enableLocalMemory,
                                              DebugManager.flags.EnableForcePin.get(),
                                              true,
                                              executionEnvironment);
}
} // namespace OCLRT
