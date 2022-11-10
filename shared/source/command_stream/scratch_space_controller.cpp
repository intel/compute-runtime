/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
ScratchSpaceController::ScratchSpaceController(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : rootDeviceIndex(rootDeviceIndex), executionEnvironment(environment), csrAllocationStorage(allocationStorage) {

    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
    auto &coreHelper = rootDeviceEnvironment.getHelper<CoreHelper>();
    computeUnitsUsedForScratch = coreHelper.getComputeUnitsUsedForScratch(rootDeviceEnvironment);
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
