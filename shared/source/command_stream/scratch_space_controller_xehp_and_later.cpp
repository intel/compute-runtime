/*
 * Copyright (C) 2021-2022 Intel Corporation
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
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {
ScratchSpaceControllerXeHPAndLater::ScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                                                       ExecutionEnvironment &environment,
                                                                       InternalAllocationStorage &allocationStorage)
    : ScratchSpaceController(rootDeviceIndex, environment, allocationStorage) {
    auto &hwHelper = HwHelper::get(environment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->platform.eRenderCoreFamily);
    singleSurfaceStateSize = hwHelper.getRenderSurfaceStateSize();
    if (DebugManager.flags.EnablePrivateScratchSlot1.get() != -1) {
        privateScratchSpaceSupported = !!DebugManager.flags.EnablePrivateScratchSlot1.get();
    }
    if (privateScratchSpaceSupported) {
        ScratchSpaceControllerXeHPAndLater::stateSlotsCount *= 2;
    }
}

void ScratchSpaceControllerXeHPAndLater::setNewSshPtr(void *newSsh, bool &cfeDirty, bool changeId) {
    if (surfaceStateHeap != newSsh) {
        surfaceStateHeap = static_cast<char *>(newSsh);
        if (scratchAllocation == nullptr) {
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
                                                                 uint32_t requiredPerThreadScratchSize,
                                                                 uint32_t requiredPerThreadPrivateScratchSize,
                                                                 uint32_t currentTaskCount,
                                                                 OsContext &osContext,
                                                                 bool &stateBaseAddressDirty,
                                                                 bool &vfeStateDirty) {
    setNewSshPtr(sshBaseAddress, vfeStateDirty, offset == 0 ? true : false);
    bool scratchSurfaceDirty = false;
    prepareScratchAllocation(requiredPerThreadScratchSize, requiredPerThreadPrivateScratchSize, currentTaskCount, osContext, stateBaseAddressDirty, scratchSurfaceDirty, vfeStateDirty);
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
    UNRECOVERABLE_IF(scratchAllocation == nullptr && privateScratchAllocation == nullptr);

    void *surfaceStateForScratchAllocation = ptrOffset(static_cast<void *>(surfaceStateHeap), getOffsetToSurfaceState(slotId + sshOffset));
    programSurfaceStateAtPtr(surfaceStateForScratchAllocation);
}

void ScratchSpaceControllerXeHPAndLater::programSurfaceStateAtPtr(void *surfaceStateForScratchAllocation) {
    auto &hwHelper = HwHelper::get(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->platform.eRenderCoreFamily);
    uint64_t scratchAllocationAddress = 0u;
    if (scratchAllocation) {
        scratchAllocationAddress = scratchAllocation->getGpuAddress();
    }
    hwHelper.setRenderSurfaceStateForBuffer(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex],
                                            surfaceStateForScratchAllocation, computeUnitsUsedForScratch, scratchAllocationAddress, 0,
                                            perThreadScratchSize, nullptr, false, scratchType, false, true);

    if (privateScratchSpaceSupported) {
        void *surfaceStateForPrivateScratchAllocation = ptrOffset(surfaceStateForScratchAllocation, singleSurfaceStateSize);
        uint64_t privateScratchAllocationAddress = 0u;

        if (privateScratchAllocation) {
            privateScratchAllocationAddress = privateScratchAllocation->getGpuAddress();
        }
        hwHelper.setRenderSurfaceStateForBuffer(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex],
                                                surfaceStateForPrivateScratchAllocation, computeUnitsUsedForScratch,
                                                privateScratchAllocationAddress, 0, perThreadPrivateScratchSize, nullptr, false,
                                                scratchType, false, true);
    }
}

uint64_t ScratchSpaceControllerXeHPAndLater::calculateNewGSH() {
    return 0u;
}
uint64_t ScratchSpaceControllerXeHPAndLater::getScratchPatchAddress() {
    uint64_t scratchAddress = 0u;
    if (scratchAllocation || privateScratchAllocation) {
        if (ApiSpecificConfig::getBindlessConfiguration()) {
            scratchAddress = bindlessSS.surfaceStateOffset;
        } else {
            scratchAddress = static_cast<uint64_t>(getOffsetToSurfaceState(slotId + sshOffset));
        }
    }
    return scratchAddress;
}

size_t ScratchSpaceControllerXeHPAndLater::getOffsetToSurfaceState(uint32_t requiredSlotCount) const {
    auto offset = requiredSlotCount * singleSurfaceStateSize;
    if (privateScratchSpaceSupported) {
        offset *= 2;
    }
    return offset;
}

void ScratchSpaceControllerXeHPAndLater::reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) {
    if (heapType == IndirectHeap::Type::SURFACE_STATE) {
        indirectHeap->getSpace(getOffsetToSurfaceState(stateSlotsCount));
    }
}

void ScratchSpaceControllerXeHPAndLater::programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                                               uint32_t requiredPerThreadScratchSize,
                                                                               uint32_t requiredPerThreadPrivateScratchSize,
                                                                               uint32_t currentTaskCount,
                                                                               OsContext &osContext,
                                                                               bool &stateBaseAddressDirty,
                                                                               bool &vfeStateDirty,
                                                                               NEO::CommandStreamReceiver *csr) {
    bool scratchSurfaceDirty = false;
    prepareScratchAllocation(requiredPerThreadScratchSize, requiredPerThreadPrivateScratchSize, currentTaskCount, osContext, stateBaseAddressDirty, scratchSurfaceDirty, vfeStateDirty);
    if (scratchSurfaceDirty) {
        bindlessSS = heapsHelper->allocateSSInHeap(singleSurfaceStateSize * (privateScratchSpaceSupported ? 2 : 1), scratchAllocation, BindlessHeapsHelper::SCRATCH_SSH);
        programSurfaceStateAtPtr(bindlessSS.ssPtr);
        vfeStateDirty = true;
    }
    csr->makeResident(*bindlessSS.heapAllocation);
}

void ScratchSpaceControllerXeHPAndLater::prepareScratchAllocation(uint32_t requiredPerThreadScratchSize,
                                                                  uint32_t requiredPerThreadPrivateScratchSize,
                                                                  uint32_t currentTaskCount,
                                                                  OsContext &osContext,
                                                                  bool &stateBaseAddressDirty,
                                                                  bool &scratchSurfaceDirty,
                                                                  bool &vfeStateDirty) {
    uint32_t requiredPerThreadScratchSizeAlignedUp = alignUp(requiredPerThreadScratchSize, 64);
    size_t requiredScratchSizeInBytes = requiredPerThreadScratchSizeAlignedUp * computeUnitsUsedForScratch;
    scratchSurfaceDirty = false;
    auto multiTileCapable = osContext.getNumSupportedDevices() > 1;
    if (scratchSizeBytes < requiredScratchSizeInBytes) {
        if (scratchAllocation) {
            scratchAllocation->updateTaskCount(currentTaskCount, osContext.getContextId());
            csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchAllocation), TEMPORARY_ALLOCATION);
        }
        scratchSurfaceDirty = true;
        scratchSizeBytes = requiredScratchSizeInBytes;
        perThreadScratchSize = requiredPerThreadScratchSizeAlignedUp;
        AllocationProperties properties{this->rootDeviceIndex, true, scratchSizeBytes, AllocationType::SCRATCH_SURFACE, multiTileCapable, false, osContext.getDeviceBitfield()};
        scratchAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }
    if (privateScratchSpaceSupported) {
        uint32_t requiredPerThreadPrivateScratchSizeAlignedUp = alignUp(requiredPerThreadPrivateScratchSize, 64);
        size_t requiredPrivateScratchSizeInBytes = requiredPerThreadPrivateScratchSizeAlignedUp * computeUnitsUsedForScratch;
        if (privateScratchSizeBytes < requiredPrivateScratchSizeInBytes) {
            if (privateScratchAllocation) {
                privateScratchAllocation->updateTaskCount(currentTaskCount, osContext.getContextId());
                csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(privateScratchAllocation), TEMPORARY_ALLOCATION);
            }
            privateScratchSizeBytes = requiredPrivateScratchSizeInBytes;
            perThreadPrivateScratchSize = requiredPerThreadPrivateScratchSizeAlignedUp;
            scratchSurfaceDirty = true;
            AllocationProperties properties{this->rootDeviceIndex, true, privateScratchSizeBytes, AllocationType::PRIVATE_SURFACE, multiTileCapable, false, osContext.getDeviceBitfield()};
            privateScratchAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        }
    }
}

void ScratchSpaceControllerXeHPAndLater::programHeaps(HeapContainer &heapContainer,
                                                      uint32_t scratchSlot,
                                                      uint32_t requiredPerThreadScratchSize,
                                                      uint32_t requiredPerThreadPrivateScratchSize,
                                                      uint32_t currentTaskCount,
                                                      OsContext &osContext,
                                                      bool &stateBaseAddressDirty,
                                                      bool &vfeStateDirty) {
    sshOffset = scratchSlot;
    updateSlots = false;
    setRequiredScratchSpace(heapContainer[0]->getUnderlyingBuffer(), sshOffset, requiredPerThreadScratchSize, requiredPerThreadPrivateScratchSize, currentTaskCount, osContext, stateBaseAddressDirty, vfeStateDirty);

    for (uint32_t i = 1; i < heapContainer.size(); ++i) {
        surfaceStateHeap = static_cast<char *>(heapContainer[i]->getUnderlyingBuffer());
        updateSlots = false;
        programSurfaceState();
    }

    updateSlots = true;
}

} // namespace NEO
