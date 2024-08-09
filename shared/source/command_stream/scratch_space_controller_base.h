/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/scratch_space_controller.h"

namespace NEO {

class ScratchSpaceControllerBase : public ScratchSpaceController {
  public:
    ScratchSpaceControllerBase(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage);

    void setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t scratchSlot,
                                 uint32_t requiredPerThreadScratchSizeSlot0,
                                 uint32_t requiredPerThreadScratchSizeSlot1,
                                 OsContext &osContext,
                                 bool &stateBaseAddressDirty,
                                 bool &vfeStateDirty) override;

    uint64_t calculateNewGSH() override;
    uint64_t getScratchPatchAddress() override;

    void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) override;
    void programHeaps(HeapContainer &heapContainer,
                      uint32_t scratchSlot,
                      uint32_t requiredPerThreadScratchSizeSlot0,
                      uint32_t requiredPerThreadScratchSizeSlot1,
                      OsContext &osContext,
                      bool &stateBaseAddressDirty,
                      bool &vfeStateDirty) override;
    void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                               uint32_t requiredPerThreadScratchSizeSlot0,
                                               uint32_t requiredPerThreadScratchSizeSlot1,
                                               OsContext &osContext,
                                               bool &stateBaseAddressDirty,
                                               bool &vfeStateDirty,
                                               NEO::CommandStreamReceiver *csr) override;

  protected:
    void createScratchSpaceAllocation();
};
} // namespace NEO
