/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/indirect_heap/indirect_heap.h"
#include <cstddef>
#include <cstdint>

namespace OCLRT {

class Device;
class ExecutionEnvironment;
class GraphicsAllocation;
class InternalAllocationStorage;
class MemoryManager;
struct HardwareInfo;

class ScratchSpaceController {
  public:
    ScratchSpaceController(const HardwareInfo &info, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage);
    virtual ~ScratchSpaceController();

    GraphicsAllocation *getScratchSpaceAllocation() {
        return scratchAllocation;
    }
    virtual void setRequiredScratchSpace(void *sshBaseAddress,
                                         uint32_t requiredPerThreadScratchSize,
                                         uint32_t currentTaskCount,
                                         uint32_t deviceIdx,
                                         bool &stateBaseAddressDirty,
                                         bool &vfeStateDirty) = 0;
    virtual uint64_t calculateNewGSH() = 0;
    virtual uint64_t getScratchPatchAddress() = 0;

    virtual void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) = 0;

  protected:
    MemoryManager *getMemoryManager() const;

    const HardwareInfo &hwInfo;
    ExecutionEnvironment &executionEnvironment;
    GraphicsAllocation *scratchAllocation = nullptr;
    InternalAllocationStorage &csrAllocationStorage;
    size_t scratchSizeBytes = 0;
    bool force32BitAllocation = false;
    uint32_t computeUnitsUsedForScratch = 0;
};
} // namespace OCLRT
