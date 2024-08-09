/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
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
inline constexpr size_t scratchSpaceOffsetFor64Bit = 4096u;
}

using ResidencyContainer = std::vector<GraphicsAllocation *>;

class ScratchSpaceController : NonCopyableOrMovableClass {
  public:
    ScratchSpaceController(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage);
    virtual ~ScratchSpaceController();

    MOCKABLE_VIRTUAL GraphicsAllocation *getScratchSpaceSlot0Allocation() {
        return scratchSlot0Allocation;
    }
    GraphicsAllocation *getScratchSpaceSlot1Allocation() {
        return scratchSlot1Allocation;
    }
    virtual void setRequiredScratchSpace(void *sshBaseAddress,
                                         uint32_t scratchSlot,
                                         uint32_t requiredPerThreadScratchSizeSlot0,
                                         uint32_t requiredPerThreadScratchSizeSlot1,
                                         OsContext &osContext,
                                         bool &stateBaseAddressDirty,
                                         bool &vfeStateDirty) = 0;

    virtual uint64_t calculateNewGSH() = 0;
    virtual uint64_t getScratchPatchAddress() = 0;
    inline uint32_t getPerThreadScratchSpaceSizeSlot0() {
        return static_cast<uint32_t>(scratchSlot0SizeInBytes / computeUnitsUsedForScratch);
    }
    inline uint32_t getPerThreadScratchSizeSlot1() {
        return static_cast<uint32_t>(scratchSlot1SizeInBytes / computeUnitsUsedForScratch);
    }

    virtual void reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) = 0;
    virtual void programHeaps(HeapContainer &heapContainer,
                              uint32_t scratchSlot,
                              uint32_t requiredPerThreadScratchSizeSlot0,
                              uint32_t requiredPerThreadScratchSizeSlot1,
                              OsContext &osContext,
                              bool &stateBaseAddressDirty,
                              bool &vfeStateDirty) = 0;
    virtual void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                       uint32_t requiredPerThreadScratchSizeSlot0,
                                                       uint32_t requiredPerThreadScratchSizeSlot1,
                                                       OsContext &osContext,
                                                       bool &stateBaseAddressDirty,
                                                       bool &vfeStateDirty,
                                                       CommandStreamReceiver *csr) = 0;

  protected:
    MemoryManager *getMemoryManager() const;

    const uint32_t rootDeviceIndex;
    ExecutionEnvironment &executionEnvironment;
    GraphicsAllocation *scratchSlot0Allocation = nullptr;
    GraphicsAllocation *scratchSlot1Allocation = nullptr;
    InternalAllocationStorage &csrAllocationStorage;
    size_t scratchSlot0SizeInBytes = 0;
    size_t scratchSlot1SizeInBytes = 0;
    bool force32BitAllocation = false;
    uint32_t computeUnitsUsedForScratch = 0;
};
} // namespace NEO
