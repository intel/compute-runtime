/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/scratch_space_controller.h"

#include "core/execution_environment/execution_environment.h"
#include "core/helpers/hw_helper.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {
ScratchSpaceController::ScratchSpaceController(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : rootDeviceIndex(rootDeviceIndex), executionEnvironment(environment), csrAllocationStorage(allocationStorage) {
    auto hwInfo = executionEnvironment.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    computeUnitsUsedForScratch = hwHelper.getComputeUnitsUsedForScratch(hwInfo);
}

ScratchSpaceController::~ScratchSpaceController() {
    if (scratchAllocation) {
        getMemoryManager()->freeGraphicsMemory(scratchAllocation);
    }
    if (privateScratchAllocation) {
        getMemoryManager()->freeGraphicsMemory(privateScratchAllocation);
    }
}

MemoryManager *ScratchSpaceController::getMemoryManager() const {
    UNRECOVERABLE_IF(executionEnvironment.memoryManager.get() == nullptr);
    return executionEnvironment.memoryManager.get();
}
} // namespace NEO
