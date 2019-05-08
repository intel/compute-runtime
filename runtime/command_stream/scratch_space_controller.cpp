/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/scratch_space_controller.h"

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {
ScratchSpaceController::ScratchSpaceController(ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : executionEnvironment(environment), csrAllocationStorage(allocationStorage) {
    auto hwInfo = executionEnvironment.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    computeUnitsUsedForScratch = hwHelper.getComputeUnitsUsedForScratch(hwInfo);
}

ScratchSpaceController::~ScratchSpaceController() {
    if (scratchAllocation) {
        getMemoryManager()->freeGraphicsMemory(scratchAllocation);
    }
}

MemoryManager *ScratchSpaceController::getMemoryManager() const {
    UNRECOVERABLE_IF(executionEnvironment.memoryManager.get() == nullptr);
    return executionEnvironment.memoryManager.get();
}
} // namespace NEO
