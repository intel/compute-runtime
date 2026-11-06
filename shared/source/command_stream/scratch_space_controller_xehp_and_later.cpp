/*
 * Copyright (C) 2021-2026 Intel Corporation
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
#include "shared/source/os_interface/product_helper.h"

#include <utility>

namespace NEO {
ScratchSpaceControllerXeHPAndLater::ScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                                                       ExecutionEnvironment &environment,
                                                                       InternalAllocationStorage &allocationStorage)
    : ScratchSpaceController(rootDeviceIndex, environment, allocationStorage) {
    auto &rootDeviceEnvironment = *environment.rootDeviceEnvironments[rootDeviceIndex];
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    singleSurfaceStateSize = gfxCoreHelper.getScratchSurfaceStateSize(rootDeviceEnvironment);
    if (debugManager.flags.EnablePrivateScratchSlot1.get() != -1) {
        twoSlotScratchSpaceSupported = !!debugManager.flags.EnablePrivateScratchSlot1.get();
    }
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto scratchSizesCount = Math::log2(gfxCoreHelper.getMaxScratchSize(productHelper)) - Math::log2(gfxCoreHelper.getMinimalScratchSpaceSize()) + 1;
    auto maxScratchGrowths = scratchSizesCount * (twoSlotScratchSpaceSupported ? 2 : 1);
    // Every scratch growth takes a new slot, slot 0 is never used and programHeaps() offsets slots by one.
    // A power of two keeps the reserved heap space aligned.
    stateSlotsCount = Math::nextPowerOfTwo(maxScratchGrowths + 2);
}

void ScratchSpaceControllerXeHPAndLater::setNewSshPtr(void *newSsh, bool &cfeDirty) {
    if (surfaceStateHeap != newSsh) {
        surfaceStateHeap = static_cast<char *>(newSsh);
        if (scratchSlot0Allocation == nullptr && scratchSlot1Allocation == nullptr) {
            cfeDirty = false;
        } else {
            programSurfaceState();
            cfeDirty = true;
        }
    }
}

bool ScratchSpaceControllerXeHPAndLater::setRequiredScratchSpace(void *sshBaseAddress,
                                                                 uint32_t offset,
                                                                 uint32_t requiredPerThreadScratchSizeSlot0,
                                                                 uint32_t requiredPerThreadScratchSizeSlot1,
                                                                 OsContext &osContext,
                                                                 bool &stateBaseAddressDirty,
                                                                 bool &vfeStateDirty) {
    if (!reserveScratchSpace(requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext)) {
        return false;
    }
    if (std::exchange(scratchSurfaceStateProgrammingPending, false)) {
        surfaceStateHeap = static_cast<char *>(sshBaseAddress);
        vfeStateDirty = true;
        slotId++;
        programSurfaceState();
    } else {
        setNewSshPtr(sshBaseAddress, vfeStateDirty);
    }
    return true;
}

bool ScratchSpaceControllerXeHPAndLater::reserveScratchSpace(uint32_t requiredPerThreadScratchSizeSlot0,
                                                             uint32_t requiredPerThreadScratchSizeSlot1,
                                                             OsContext &osContext) {
    bool scratchSurfaceDirty = false;
    if (!prepareScratchAllocation(requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext, scratchSurfaceDirty)) {
        return false;
    }
    scratchSurfaceStateProgrammingPending |= scratchSurfaceDirty;
    return true;
}

void ScratchSpaceControllerXeHPAndLater::programSurfaceState() {
    UNRECOVERABLE_IF(slotId + sshOffset >= stateSlotsCount);
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
                                                          perThreadScratchSpaceSlot0Size, nullptr, false, scratchType, false, true);

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

size_t ScratchSpaceControllerXeHPAndLater::getReservedSshSize() const {
    return getOffsetToSurfaceState(stateSlotsCount);
}

void ScratchSpaceControllerXeHPAndLater::reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) {
    if (heapType == IndirectHeap::Type::surfaceState) {
        indirectHeap->getSpace(getReservedSshSize());
    }
}

void ScratchSpaceControllerXeHPAndLater::programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                                               uint32_t requiredPerThreadScratchSizeSlot0,
                                                                               uint32_t requiredPerThreadScratchSizeSlot1,
                                                                               OsContext &osContext,
                                                                               bool &stateBaseAddressDirty,
                                                                               bool &vfeStateDirty,
                                                                               NEO::CommandStreamReceiver *csr) {
    if (reserveScratchSpace(requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext) &&
        std::exchange(scratchSurfaceStateProgrammingPending, false)) {
        bindlessSS = heapsHelper->allocateSSInHeap(singleSurfaceStateSize * (twoSlotScratchSpaceSupported ? 2 : 1), scratchSlot0Allocation, BindlessHeapsHelper::specialSsh);
        UNRECOVERABLE_IF(bindlessSS.ssPtr == nullptr);
        programSurfaceStateAtPtr(bindlessSS.ssPtr);
        vfeStateDirty = true;
    }
    if (bindlessSS.heapAllocation) {
        csr->makeResident(*bindlessSS.heapAllocation);
    }
}

bool ScratchSpaceControllerXeHPAndLater::prepareScratchAllocation(uint32_t requiredPerThreadScratchSizeSlot0,
                                                                  uint32_t requiredPerThreadScratchSizeSlot1,
                                                                  OsContext &osContext,
                                                                  bool &scratchSurfaceDirty) {
    uint32_t requiredPerThreadScratchSizeSlot0AlignedUp = requiredPerThreadScratchSizeSlot0;
    if (!Math::isPow2(requiredPerThreadScratchSizeSlot0AlignedUp)) {
        requiredPerThreadScratchSizeSlot0AlignedUp = Math::nextPowerOfTwo(requiredPerThreadScratchSizeSlot0);
    }
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    size_t requiredScratchSizeInBytes = static_cast<size_t>(requiredPerThreadScratchSizeSlot0AlignedUp) * computeUnitsUsedForScratch;
    productHelper.adjustScratchSize(requiredScratchSizeInBytes);
    scratchSurfaceDirty = false;
    const bool slot0GrowthRequired = scratchSlot0SizeInBytes < requiredScratchSizeInBytes;

    uint32_t requiredPerThreadScratchSizeSlot1AlignedUp = 0u;
    size_t requiredScratchSlot1SizeInBytes = 0u;
    if (twoSlotScratchSpaceSupported) {
        requiredPerThreadScratchSizeSlot1AlignedUp = requiredPerThreadScratchSizeSlot1;
        if (!Math::isPow2(requiredPerThreadScratchSizeSlot1AlignedUp)) {
            requiredPerThreadScratchSizeSlot1AlignedUp = Math::nextPowerOfTwo(requiredPerThreadScratchSizeSlot1);
        }
        requiredScratchSlot1SizeInBytes = static_cast<size_t>(requiredPerThreadScratchSizeSlot1AlignedUp) * computeUnitsUsedForScratch;
        productHelper.adjustScratchSize(requiredScratchSlot1SizeInBytes);
    }
    const bool slot1GrowthRequired = twoSlotScratchSpaceSupported && (scratchSlot1SizeInBytes < requiredScratchSlot1SizeInBytes);

    GraphicsAllocation *newSlot0Allocation = nullptr;
    if (slot0GrowthRequired) {
        newSlot0Allocation = allocateScratchSurface(requiredScratchSizeInBytes, osContext);
        if (newSlot0Allocation == nullptr) {
            return false;
        }
    }
    GraphicsAllocation *newSlot1Allocation = nullptr;
    if (slot1GrowthRequired) {
        newSlot1Allocation = allocateScratchSurface(requiredScratchSlot1SizeInBytes, osContext);
        if (newSlot1Allocation == nullptr) {
            getMemoryManager()->freeGraphicsMemory(newSlot0Allocation);
            return false;
        }
    }

    if (slot0GrowthRequired) {
        if (scratchSlot0Allocation) {
            csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchSlot0Allocation), TEMPORARY_ALLOCATION);
        }
        scratchSlot0Allocation = newSlot0Allocation;
        scratchSlot0SizeInBytes = requiredScratchSizeInBytes;
        perThreadScratchSpaceSlot0Size = requiredPerThreadScratchSizeSlot0AlignedUp;
        scratchSurfaceDirty = true;
    }
    if (slot1GrowthRequired) {
        if (scratchSlot1Allocation) {
            csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchSlot1Allocation), TEMPORARY_ALLOCATION);
        }
        scratchSlot1Allocation = newSlot1Allocation;
        scratchSlot1SizeInBytes = requiredScratchSlot1SizeInBytes;
        perThreadScratchSpaceSlot1Size = requiredPerThreadScratchSizeSlot1AlignedUp;
        scratchSurfaceDirty = true;
    }
    return true;
}

GraphicsAllocation *ScratchSpaceControllerXeHPAndLater::allocateScratchSurface(size_t scratchSizeInBytes, OsContext &osContext) {
    const bool multiTileCapable = osContext.getNumSupportedDevices() > 1;
    AllocationProperties properties{this->rootDeviceIndex, true, scratchSizeInBytes, AllocationType::scratchSurface, multiTileCapable, false, osContext.getDeviceBitfield()};
    return getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
}

bool ScratchSpaceControllerXeHPAndLater::programHeaps(HeapContainer &heapContainer,
                                                      uint32_t scratchSlot,
                                                      uint32_t requiredPerThreadScratchSizeSlot0,
                                                      uint32_t requiredPerThreadScratchSizeSlot1,
                                                      OsContext &osContext,
                                                      bool &stateBaseAddressDirty,
                                                      bool &vfeStateDirty) {
    sshOffset = scratchSlot;
    if (!setRequiredScratchSpace(heapContainer[0]->getUnderlyingBuffer(), sshOffset, requiredPerThreadScratchSizeSlot0, requiredPerThreadScratchSizeSlot1, osContext, stateBaseAddressDirty, vfeStateDirty)) {
        return false;
    }

    for (uint32_t i = 1; i < heapContainer.size(); ++i) {
        surfaceStateHeap = static_cast<char *>(heapContainer[i]->getUnderlyingBuffer());
        programSurfaceState();
    }
    return true;
}

} // namespace NEO
