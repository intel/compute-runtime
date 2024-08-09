/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {
ScratchSpaceControllerXeHPAndLater::ScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                                                       ExecutionEnvironment &environment,
                                                                       InternalAllocationStorage &allocationStorage)
    : ScratchSpaceController(rootDeviceIndex, environment, allocationStorage) {
    auto &gfxCoreHelper = environment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
    singleSurfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
    if (debugManager.flags.EnablePrivateScratchSlot1.get() != -1) {
        twoSlotScratchSpaceSupported = !!debugManager.flags.EnablePrivateScratchSlot1.get();
    }
    if (twoSlotScratchSpaceSupported) {
        ScratchSpaceControllerXeHPAndLater::stateSlotsCount *= 2;
    }
}

void ScratchSpaceControllerXeHPAndLater::setNewSshPtr(void *newSsh, bool &cfeDirty, bool changeId) {
    if (surfaceStateHeap != newSsh) {
        surfaceStateHeap = static_cast<char *>(newSsh);
        if (scratchSlot0Allocation == nullptr) {
            cfeDirty = false;
        } else {
            if (changeId) {
                slotId = 0;
            }
            programSurfaceState();
            cfeDirty = true;
        }
    }
}

void ScratchSpaceControllerXeHPAndLater::setRequiredScratchSpace(void *sshBaseAddress,
                                                                 uint32_t offset,
                                                                 uint32_t requiredPerThreadScratchSizeSlot0,
                                                                 uint32_t requiredPerThreadScratchSizeSlot1,
                                                                 OsContext &osContext,
                                                                 bool &stateBaseAddressDirty,
                                                                 bool &vfeStateDirty) {
    setNewSshPtr(sshBaseAddress, vfeStateDirty, offset == 0 ? true : false);
    bool scratchSurfaceDirty = false;
    prepareScratchAllocation(requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext, stateBaseAddressDirty, scratchSurfaceDirty, vfeStateDirty);
    if (scratchSurfaceDirty) {
        vfeStateDirty = true;
        updateSlots = true;
        programSurfaceState();
    }
}

void ScratchSpaceControllerXeHPAndLater::programSurfaceState() {
    if (updateSlots) {
        slotId++;
    }
    UNRECOVERABLE_IF(slotId >= stateSlotsCount);
    UNRECOVERABLE_IF(scratchSlot0Allocation == nullptr && scratchSlot1Allocation == nullptr);

    void *surfaceStateForScratchAllocation = ptrOffset(static_cast<void *>(surfaceStateHeap), getOffsetToSurfaceState(slotId + sshOffset));
    programSurfaceStateAtPtr(surfaceStateForScratchAllocation);
}

void ScratchSpaceControllerXeHPAndLater::programSurfaceStateAtPtr(void *surfaceStateForScratchAllocation) {
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
    uint64_t scratchAllocationAddress = 0u;
    if (scratchSlot0Allocation) {
        scratchAllocationAddress = scratchSlot0Allocation->getGpuAddress();
    }
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex],
                                                          surfaceStateForScratchAllocation, computeUnitsUsedForScratch, scratchAllocationAddress, 0,
                                                          perThreadScratchSize, nullptr, false, scratchType, false, true);

    if (twoSlotScratchSpaceSupported) {
        void *surfaceStateForSlot1Allocation = ptrOffset(surfaceStateForScratchAllocation, singleSurfaceStateSize);
        uint64_t scratchSlot1AllocationAddress = 0u;

        if (scratchSlot1Allocation) {
            scratchSlot1AllocationAddress = scratchSlot1Allocation->getGpuAddress();
        }
        gfxCoreHelper.setRenderSurfaceStateForScratchResource(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex],
                                                              surfaceStateForSlot1Allocation, computeUnitsUsedForScratch,
                                                              scratchSlot1AllocationAddress, 0, perThreadScratchSpaceSlot1Size, nullptr, false,
                                                              scratchType, false, true);
    }
}

uint64_t ScratchSpaceControllerXeHPAndLater::calculateNewGSH() {
    return 0u;
}
uint64_t ScratchSpaceControllerXeHPAndLater::getScratchPatchAddress() {
    uint64_t scratchAddress = 0u;
    if (scratchSlot0Allocation || scratchSlot1Allocation) {
        scratchAddress = static_cast<uint64_t>(getOffsetToSurfaceState(slotId + sshOffset));
    }
    return scratchAddress;
}

size_t ScratchSpaceControllerXeHPAndLater::getOffsetToSurfaceState(uint32_t requiredSlotCount) const {
    auto offset = requiredSlotCount * singleSurfaceStateSize;
    if (twoSlotScratchSpaceSupported) {
        offset *= 2;
    }
    return offset;
}

void ScratchSpaceControllerXeHPAndLater::reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) {
    if (heapType == IndirectHeap::Type::surfaceState) {
        indirectHeap->getSpace(getOffsetToSurfaceState(stateSlotsCount));
    }
}

void ScratchSpaceControllerXeHPAndLater::programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                                               uint32_t requiredPerThreadScratchSizeSlot0,
                                                                               uint32_t requiredPerThreadScratchSizeSlot1,
                                                                               OsContext &osContext,
                                                                               bool &stateBaseAddressDirty,
                                                                               bool &vfeStateDirty,
                                                                               NEO::CommandStreamReceiver *csr) {
    bool scratchSurfaceDirty = false;
    prepareScratchAllocation(requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext, stateBaseAddressDirty, scratchSurfaceDirty, vfeStateDirty);
    if (scratchSurfaceDirty) {
        bindlessSS = heapsHelper->allocateSSInHeap(singleSurfaceStateSize * (twoSlotScratchSpaceSupported ? 2 : 1), scratchSlot0Allocation, BindlessHeapsHelper::specialSsh);
        programSurfaceStateAtPtr(bindlessSS.ssPtr);
        vfeStateDirty = true;
    }
    if (bindlessSS.heapAllocation) {
        csr->makeResident(*bindlessSS.heapAllocation);
    }
}

void ScratchSpaceControllerXeHPAndLater::prepareScratchAllocation(uint32_t requiredPerThreadScratchSizeSlot0,
                                                                  uint32_t requiredPerThreadScratchSizeSlot1,
                                                                  OsContext &osContext,
                                                                  bool &stateBaseAddressDirty,
                                                                  bool &scratchSurfaceDirty,
                                                                  bool &vfeStateDirty) {
    uint32_t requiredPerThreadScratchSizeSlot0AlignedUp = requiredPerThreadScratchSizeSlot0;
    if (!Math::isPow2(requiredPerThreadScratchSizeSlot0AlignedUp)) {
        requiredPerThreadScratchSizeSlot0AlignedUp = Math::nextPowerOfTwo(requiredPerThreadScratchSizeSlot0);
    }
    size_t requiredScratchSizeInBytes = static_cast<size_t>(requiredPerThreadScratchSizeSlot0AlignedUp) * computeUnitsUsedForScratch;
    scratchSurfaceDirty = false;
    auto multiTileCapable = osContext.getNumSupportedDevices() > 1;
    if (scratchSlot0SizeInBytes < requiredScratchSizeInBytes) {
        if (scratchSlot0Allocation) {
            csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchSlot0Allocation), TEMPORARY_ALLOCATION);
        }
        scratchSurfaceDirty = true;
        scratchSlot0SizeInBytes = requiredScratchSizeInBytes;
        perThreadScratchSize = requiredPerThreadScratchSizeSlot0AlignedUp;
        AllocationProperties properties{this->rootDeviceIndex, true, scratchSlot0SizeInBytes, AllocationType::scratchSurface, multiTileCapable, false, osContext.getDeviceBitfield()};
        scratchSlot0Allocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }
    if (twoSlotScratchSpaceSupported) {
        uint32_t requiredPerThreadScratchSizeSlot1AlignedUp = requiredPerThreadScratchSizeSlot1;
        if (!Math::isPow2(requiredPerThreadScratchSizeSlot1AlignedUp)) {
            requiredPerThreadScratchSizeSlot1AlignedUp = Math::nextPowerOfTwo(requiredPerThreadScratchSizeSlot1);
        }
        size_t requiredScratchSlot1SizeInBytes = static_cast<size_t>(requiredPerThreadScratchSizeSlot1AlignedUp) * computeUnitsUsedForScratch;
        if (scratchSlot1SizeInBytes < requiredScratchSlot1SizeInBytes) {
            if (scratchSlot1Allocation) {
                csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchSlot1Allocation), TEMPORARY_ALLOCATION);
            }
            scratchSlot1SizeInBytes = requiredScratchSlot1SizeInBytes;
            perThreadScratchSpaceSlot1Size = requiredPerThreadScratchSizeSlot1AlignedUp;
            scratchSurfaceDirty = true;
            AllocationProperties properties{this->rootDeviceIndex, true, scratchSlot1SizeInBytes, AllocationType::scratchSurface, multiTileCapable, false, osContext.getDeviceBitfield()};
            scratchSlot1Allocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        }
    }
}

void ScratchSpaceControllerXeHPAndLater::programHeaps(HeapContainer &heapContainer,
                                                      uint32_t scratchSlot,
                                                      uint32_t requiredPerThreadScratchSizeSlot0,
                                                      uint32_t requiredPerThreadScratchSizeSlot1,
                                                      OsContext &osContext,
                                                      bool &stateBaseAddressDirty,
                                                      bool &vfeStateDirty) {
    sshOffset = scratchSlot;
    updateSlots = false;
    setRequiredScratchSpace(heapContainer[0]->getUnderlyingBuffer(), sshOffset, requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext, stateBaseAddressDirty, vfeStateDirty);

    for (uint32_t i = 1; i < heapContainer.size(); ++i) {
        surfaceStateHeap = static_cast<char *>(heapContainer[i]->getUnderlyingBuffer());
        updateSlots = false;
        programSurfaceState();
    }

    updateSlots = true;
}

} // namespace NEO
