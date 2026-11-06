/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include <utility>

namespace NEO {
ScratchSpaceControllerBase::ScratchSpaceControllerBase(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : ScratchSpaceController(rootDeviceIndex, environment, allocationStorage) {
}

bool ScratchSpaceControllerBase::setRequiredScratchSpace(void *sshBaseAddress,
                                                         uint32_t scratchSlot,
                                                         uint32_t requiredPerThreadScratchSizeSlot0,
                                                         uint32_t requiredPerThreadScratchSizeSlot1,
                                                         OsContext &osContext,
                                                         bool &stateBaseAddressDirty,
                                                         bool &vfeStateDirty) {
    if (!reserveScratchSpace(requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext)) {
        return false;
    }
    if (std::exchange(scratchSurfaceStateProgrammingPending, false)) {
        vfeStateDirty = true;
        if (is64bit) {
            stateBaseAddressDirty = true;
        }
    }
    return true;
}

bool ScratchSpaceControllerBase::reserveScratchSpace(uint32_t requiredPerThreadScratchSizeSlot0,
                                                     uint32_t requiredPerThreadScratchSizeSlot1,
                                                     OsContext &osContext) {
    if (requiredPerThreadScratchSizeSlot0 && (perThreadScratchSpaceSlot0Size < requiredPerThreadScratchSizeSlot0)) {
        const size_t requiredScratchSizeInBytes = requiredPerThreadScratchSizeSlot0 * computeUnitsUsedForScratch;
        auto newScratchAllocation = createScratchSpaceAllocation(requiredScratchSizeInBytes);
        if (newScratchAllocation == nullptr) {
            return false;
        }
        if (scratchSlot0Allocation) {
            csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchSlot0Allocation), TEMPORARY_ALLOCATION);
        }
        scratchSlot0Allocation = newScratchAllocation;
        perThreadScratchSpaceSlot0Size = requiredPerThreadScratchSizeSlot0;
        scratchSlot0SizeInBytes = requiredScratchSizeInBytes;
        scratchSurfaceStateProgrammingPending = true;
    }
    return true;
}

GraphicsAllocation *ScratchSpaceControllerBase::createScratchSpaceAllocation(size_t scratchSizeInBytes) {
    return getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, scratchSizeInBytes, AllocationType::scratchSurface, this->csrAllocationStorage.getDeviceBitfield()});
}

uint64_t ScratchSpaceControllerBase::calculateNewGSH() {
    uint64_t gsh = 0;
    if (scratchSlot0Allocation) {
        gsh = scratchSlot0Allocation->getGpuAddress() - ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
    }
    return gsh;
}
uint64_t ScratchSpaceControllerBase::getScratchPatchAddress() {
    // for 32 bit scratch space pointer is being programmed in Media VFE State and is relative to 0 as General State Base Address
    // for 64 bit, scratch space pointer is being programmed as "General State Base Address - scratchSpaceOffsetFor64bit"
    //             and "0 + scratchSpaceOffsetFor64bit" is being programmed in Media VFE state
    uint64_t scratchAddress = 0;
    if (scratchSlot0Allocation) {
        if (is64bit) {
            // this is to avoid scratch allocation offset "0"
            scratchAddress = ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
        } else {
            scratchAddress = scratchSlot0Allocation->getGpuAddressToPatch();
        }
    }
    return scratchAddress;
}

size_t ScratchSpaceControllerBase::getReservedSshSize() const {
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
    return rootDeviceEnvironment.getHelper<GfxCoreHelper>().getRenderSurfaceStateSize(rootDeviceEnvironment);
}

void ScratchSpaceControllerBase::reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) {
    if (heapType == IndirectHeap::Type::surfaceState) {
        indirectHeap->getSpace(getReservedSshSize());
    }
}

bool ScratchSpaceControllerBase::programHeaps(HeapContainer &heapContainer,
                                              uint32_t offset,
                                              uint32_t requiredPerThreadScratchSizeSlot0,
                                              uint32_t requiredPerThreadScratchSizeSlot1,
                                              OsContext &osContext,
                                              bool &stateBaseAddressDirty,
                                              bool &vfeStateDirty) {
    return true;
}

void ScratchSpaceControllerBase::programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                                       uint32_t requiredPerThreadScratchSizeSlot0,
                                                                       uint32_t requiredPerThreadScratchSizeSlot1,
                                                                       OsContext &osContext,
                                                                       bool &stateBaseAddressDirty,
                                                                       bool &vfeStateDirty,
                                                                       NEO::CommandStreamReceiver *csr) {
}
} // namespace NEO
