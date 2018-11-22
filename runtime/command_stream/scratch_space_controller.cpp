/*
 * Copyright (C) 2018 Intel Corporation
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

namespace OCLRT {
ScratchSpaceController::ScratchSpaceController(const HardwareInfo &info, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : hwInfo(info), executionEnvironment(environment), csrAllocationStorage(allocationStorage) {
    auto &hwHelper = HwHelper::get(info.pPlatform->eRenderCoreFamily);
    computeUnitsUsedForScratch = hwHelper.getComputeUnitsUsedForScratch(&hwInfo);
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
} // namespace OCLRT
