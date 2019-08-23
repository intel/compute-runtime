/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/scratch_space_controller_base.h"

#include "core/helpers/aligned_memory.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/preamble.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {
ScratchSpaceControllerBase::ScratchSpaceControllerBase(ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : ScratchSpaceController(environment, allocationStorage) {
}

void ScratchSpaceControllerBase::setRequiredScratchSpace(void *sshBaseAddress,
                                                         uint32_t requiredPerThreadScratchSize,
                                                         uint32_t requiredPerThreadPrivateScratchSize,
                                                         uint32_t currentTaskCount,
                                                         uint32_t contextId,
                                                         bool &stateBaseAddressDirty,
                                                         bool &vfeStateDirty) {
    size_t requiredScratchSizeInBytes = requiredPerThreadScratchSize * computeUnitsUsedForScratch;
    if (requiredScratchSizeInBytes && (!scratchAllocation || scratchSizeBytes < requiredScratchSizeInBytes)) {
        if (scratchAllocation) {
            scratchAllocation->updateTaskCount(currentTaskCount, contextId);
            csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchAllocation), TEMPORARY_ALLOCATION);
        }
        scratchSizeBytes = requiredScratchSizeInBytes;
        createScratchSpaceAllocation();
        vfeStateDirty = true;
        force32BitAllocation = getMemoryManager()->peekForce32BitAllocations();
        if (is64bit && !force32BitAllocation) {
            stateBaseAddressDirty = true;
        }
    }
}

void ScratchSpaceControllerBase::createScratchSpaceAllocation() {
    scratchAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties({scratchSizeBytes, GraphicsAllocation::AllocationType::SCRATCH_SURFACE});
    UNRECOVERABLE_IF(scratchAllocation == nullptr);
}

uint64_t ScratchSpaceControllerBase::calculateNewGSH() {
    uint64_t gsh = 0;
    if (scratchAllocation) {
        gsh = scratchAllocation->getGpuAddress() - ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
    }
    return gsh;
}
uint64_t ScratchSpaceControllerBase::getScratchPatchAddress() {
    //for 32 bit scratch space pointer is being programmed in Media VFE State and is relative to 0 as General State Base Address
    //for 64 bit, scratch space pointer is being programmed as "General State Base Address - scratchSpaceOffsetFor64bit"
    //            and "0 + scratchSpaceOffsetFor64bit" is being programmed in Media VFE state
    uint64_t scratchAddress = 0;
    if (scratchAllocation) {
        scratchAddress = scratchAllocation->getGpuAddressToPatch();
        if (is64bit && !getMemoryManager()->peekForce32BitAllocations()) {
            //this is to avoid scractch allocation offset "0"
            scratchAddress = ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
        }
    }
    return scratchAddress;
}

void ScratchSpaceControllerBase::reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) {
}

} // namespace NEO
