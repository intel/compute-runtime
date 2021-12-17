/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class Device;
class ExecutionEnvironment;
class GraphicsAllocation;
class InternalAllocationStorage;
class MemoryManager;
struct HardwareInfo;
class OsContext;
class CommandStreamReceiver;

namespace ScratchSpaceConstants {
constexpr size_t scratchSpaceOffsetFor64Bit = 4096u;
}

using ResidencyContainer = std::vector<GraphicsAllocation *>;

class ScratchSpaceController {
  public:
    ScratchSpaceController(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage);
    virtual ~ScratchSpaceController();

    MOCKABLE_VIRTUAL GraphicsAllocation *getScratchSpaceAllocation() {
        return scratchAllocation;
    }
    GraphicsAllocation *getPrivateScratchSpaceAllocation() {
        return privateScratchAllocation;
    }
    virtual void setRequiredScratchSpace(void *sshBaseAddress,
                                         uint32_t scratchSlot,
                                         uint32_t requiredPerThreadScratchSize,
                                         uint32_t requiredPerThreadPrivateScratchSize,
                                         uint32_t currentTaskCount,
                                         OsContext &osContext,
                                         bool &stateBaseAddressDirty,
                                         bool &vfeStateDirty) = 0;

    virtual uint64_t calculateNewGSH() = 0;
    virtual uint64_t getScratchPatchAddress() = 0;
    inline uint32_t getPerThreadScratchSpaceSize() {
        return static_cast<uint32_t>(scratchSizeBytes / computeUnitsUsedForScratch);
    }
    inline uint32_t getPerThreadPrivateScratchSize() {
        return static_cast<uint32_t>(privateScratchSizeBytes / computeUnitsUsedForScratch);
    }

    virtual void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) = 0;
    virtual void programHeaps(HeapContainer &heapContainer,
                              uint32_t scratchSlot,
                              uint32_t requiredPerThreadScratchSize,
                              uint32_t requiredPerThreadPrivateScratchSize,
                              uint32_t currentTaskCount,
                              OsContext &osContext,
                              bool &stateBaseAddressDirty,
                              bool &vfeStateDirty) = 0;
    virtual void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                       uint32_t requiredPerThreadScratchSize,
                                                       uint32_t requiredPerThreadPrivateScratchSize,
                                                       uint32_t currentTaskCount,
                                                       OsContext &osContext,
                                                       bool &stateBaseAddressDirty,
                                                       bool &vfeStateDirty,
                                                       CommandStreamReceiver *csr) = 0;

  protected:
    MemoryManager *getMemoryManager() const;

    const uint32_t rootDeviceIndex;
    ExecutionEnvironment &executionEnvironment;
    GraphicsAllocation *scratchAllocation = nullptr;
    GraphicsAllocation *privateScratchAllocation = nullptr;
    InternalAllocationStorage &csrAllocationStorage;
    size_t scratchSizeBytes = 0;
    size_t privateScratchSizeBytes = 0;
    bool force32BitAllocation = false;
    uint32_t computeUnitsUsedForScratch = 0;
};
} // namespace NEO
