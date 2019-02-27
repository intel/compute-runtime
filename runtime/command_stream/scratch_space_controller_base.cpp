/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/scratch_space_controller_base.h"

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/preamble.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {
ScratchSpaceControllerBase::ScratchSpaceControllerBase(const HardwareInfo &info, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : ScratchSpaceController(info, environment, allocationStorage) {
}

void ScratchSpaceControllerBase::setRequiredScratchSpace(void *sshBaseAddress,
                                                         uint32_t requiredPerThreadScratchSize,
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
    auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);
    auto scratchSpaceOffsetFor64bit = hwHelper.getScratchSpaceOffsetFor64bit();
    return scratchAllocation->getGpuAddress() - scratchSpaceOffsetFor64bit;
}
uint64_t ScratchSpaceControllerBase::getScratchPatchAddress() {
    //for 32 bit scratch space pointer is being programmed in Media VFE State and is relative to 0 as General State Base Address
    //for 64 bit, scratch space pointer is being programmed as "General State Base Address - scratchSpaceOffsetFor64bit"
    //            and "0 + scratchSpaceOffsetFor64bit" is being programmed in Media VFE state
    uint64_t scratchAddress = 0;
    if (scratchAllocation) {
        scratchAddress = scratchAllocation->getGpuAddressToPatch();
        if (is64bit && !getMemoryManager()->peekForce32BitAllocations()) {
            auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);
            auto scratchSpaceOffsetFor64bit = hwHelper.getScratchSpaceOffsetFor64bit();
            //this is to avoid scractch allocation offset "0"
            scratchAddress = scratchSpaceOffsetFor64bit;
        }
    }
    return scratchAddress;
}

void ScratchSpaceControllerBase::reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) {
}

} // namespace OCLRT
