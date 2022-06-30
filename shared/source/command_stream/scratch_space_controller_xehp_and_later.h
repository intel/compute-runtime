/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/scratch_space_controller.h"

#include <cstdint>
#include <limits>

namespace NEO {

class ScratchSpaceControllerXeHPAndLater : public ScratchSpaceController {
  public:
    ScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                       ExecutionEnvironment &environment,
                                       InternalAllocationStorage &allocationStorage);
    void setNewSshPtr(void *newSsh, bool &cfeDirty, bool changeId);

    void setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t scratchSlot,
                                 uint32_t requiredPerThreadScratchSize,
                                 uint32_t requiredPerThreadPrivateScratchSize,
                                 uint32_t currentTaskCount,
                                 OsContext &osContext,
                                 bool &stateBaseAddressDirty,
                                 bool &vfeStateDirty) override;

    uint64_t calculateNewGSH() override;
    uint64_t getScratchPatchAddress() override;

    void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) override;

    void programHeaps(HeapContainer &heapContainer,
                      uint32_t scratchSlot,
                      uint32_t requiredPerThreadScratchSize,
                      uint32_t requiredPerThreadPrivateScratchSize,
                      uint32_t currentTaskCount,
                      OsContext &osContext,
                      bool &stateBaseAddressDirty,
                      bool &vfeStateDirty) override;
    void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                               uint32_t requiredPerThreadScratchSize,
                                               uint32_t requiredPerThreadPrivateScratchSize,
                                               uint32_t currentTaskCount,
                                               OsContext &osContext,
                                               bool &stateBaseAddressDirty,
                                               bool &vfeStateDirty,
                                               NEO::CommandStreamReceiver *csr) override;

  protected:
    MOCKABLE_VIRTUAL void programSurfaceState();
    MOCKABLE_VIRTUAL void programSurfaceStateAtPtr(void *surfaceStateForScratchAllocation);
    MOCKABLE_VIRTUAL void prepareScratchAllocation(uint32_t requiredPerThreadScratchSize,
                                                   uint32_t requiredPerThreadPrivateScratchSize,
                                                   uint32_t currentTaskCount,
                                                   OsContext &osContext,
                                                   bool &stateBaseAddressDirty,
                                                   bool &scratchSurfaceDirty,
                                                   bool &vfeStateDirty);
    size_t getOffsetToSurfaceState(uint32_t requiredSlotCount) const;

    bool updateSlots = true;
    uint32_t stateSlotsCount = 16;
    static const uint32_t scratchType = 6;
    bool privateScratchSpaceSupported = true;

    char *surfaceStateHeap = nullptr;
    size_t singleSurfaceStateSize = 0;
    uint32_t slotId = 0;
    uint32_t perThreadScratchSize = 0;
    uint32_t perThreadPrivateScratchSize = 0;
    uint32_t sshOffset = 0;
    SurfaceStateInHeapInfo bindlessSS = {};
};

} // namespace NEO
