/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/scratch_space_controller.h"

namespace OCLRT {

class ScratchSpaceControllerBase : public ScratchSpaceController {
  public:
    ScratchSpaceControllerBase(const HardwareInfo &info, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage);

    void setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t requiredPerThreadScratchSize,
                                 uint32_t currentTaskCount,
                                 uint32_t contextId,
                                 bool &stateBaseAddressDirty,
                                 bool &vfeStateDirty) override;
    uint64_t calculateNewGSH() override;
    uint64_t getScratchPatchAddress() override;

    void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) override;

  protected:
    void createScratchSpaceAllocation();
};
} // namespace OCLRT
