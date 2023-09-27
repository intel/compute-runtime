/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/in_order_cmd_helpers.h"

#include "shared/source/memory_manager/memory_manager.h"

#include <cstdint>
#include <vector>

namespace L0 {

InOrderExecInfo::~InOrderExecInfo() {
    memoryManager.freeGraphicsMemory(&inOrderDependencyCounterAllocation);
}

InOrderExecInfo::InOrderExecInfo(NEO::GraphicsAllocation &inOrderDependencyCounterAllocation, NEO::MemoryManager &memoryManager, bool isRegularCmdList)
    : inOrderDependencyCounterAllocation(inOrderDependencyCounterAllocation), memoryManager(memoryManager), isRegularCmdList(isRegularCmdList) {
}

} // namespace L0
