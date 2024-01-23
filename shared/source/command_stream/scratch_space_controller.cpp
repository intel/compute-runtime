/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
ScratchSpaceController::ScratchSpaceController(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : rootDeviceIndex(rootDeviceIndex), executionEnvironment(environment), csrAllocationStorage(allocationStorage) {

    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    computeUnitsUsedForScratch = gfxCoreHelper.getComputeUnitsUsedForScratch(rootDeviceEnvironment);
}

ScratchSpaceController::~ScratchSpaceController() {
    if (scratchSlot0Allocation) {
        getMemoryManager()->freeGraphicsMemory(scratchSlot0Allocation);
    }
    if (scratchSlot1Allocation) {
        getMemoryManager()->freeGraphicsMemory(scratchSlot1Allocation);
    }
}

MemoryManager *ScratchSpaceController::getMemoryManager() const {
    UNRECOVERABLE_IF(executionEnvironment.memoryManager.get() == nullptr);
    return executionEnvironment.memoryManager.get();
}
} // namespace NEO
