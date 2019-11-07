/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/scratch_space_controller.h"

namespace NEO {

class ScratchSpaceControllerBase : public ScratchSpaceController {
  public:
    ScratchSpaceControllerBase(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage);

    void setRequiredScratchSpace(void *sshBaseAddress,
                                 uint32_t requiredPerThreadScratchSize,
                                 uint32_t requiredPerThreadPrivateScratchSize,
                                 uint32_t currentTaskCount,
                                 OsContext &osContext,
                                 bool &stateBaseAddressDirty,
                                 bool &vfeStateDirty) override;
    uint64_t calculateNewGSH() override;
    uint64_t getScratchPatchAddress() override;

    void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) override;

  protected:
    void createScratchSpaceAllocation();
};
} // namespace NEO
