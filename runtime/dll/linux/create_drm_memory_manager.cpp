/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "core/memory_manager/memory_manager.h"
#include "core/os_interface/linux/drm_memory_manager.h"
#include "core/os_interface/linux/os_interface.h"
#include "core/os_interface/os_interface.h"

namespace NEO {
std::unique_ptr<MemoryManager> MemoryManager::createMemoryManager(ExecutionEnvironment &executionEnvironment) {
    return std::make_unique<DrmMemoryManager>(gemCloseWorkerMode::gemCloseWorkerActive,
                                              DebugManager.flags.EnableForcePin.get(),
                                              true,
                                              executionEnvironment);
}
} // namespace NEO
