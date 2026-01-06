/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace NEO {

class IndirectHeap;
struct AddressRange;
class HeapAllocator;

namespace BindlessImageSlot {
constexpr uint32_t image = 0;
constexpr uint32_t implicitArgs = 1;
constexpr uint32_t sampler = 2;
constexpr uint32_t redescribedImage = 3;
constexpr uint32_t packedImage = 4;
constexpr uint32_t max = 5;
}; // namespace BindlessImageSlot

class BindlessHeapsHelper : NEO::NonCopyableAndNonMovableClass {
  public:
    enum BindlesHeapType {
        specialSsh = 0,
        globalSsh,
        globalDsh,
        numHeapTypes
    };

    BindlessHeapsHelper(Device *rootDevice, bool isMultiOsContextCapable);
    MOCKABLE_VIRTUAL ~BindlessHeapsHelper();

    GraphicsAllocation *getHeapAllocation(size_t heapSize, size_t alignment, bool allocInFrontWindow);

    MOCKABLE_VIRTUAL SurfaceStateInHeapInfo allocateSSInHeap(size_t ssSize, GraphicsAllocation *surfaceAllocation, BindlesHeapType heapType);
    uint64_t getGlobalHeapsBase();
    void *getSpaceInHeap(size_t ssSize, BindlesHeapType heapType);
    uint32_t getDefaultBorderColorOffset();
    uint32_t getAlphaBorderColorOffset();
    IndirectHeap *getHeap(BindlesHeapType heapType);
    void releaseSSToReusePool(const SurfaceStateInHeapInfo &surfStateInfo);
    bool isGlobalDshSupported() {
        return globalBindlessDsh;
    }

    int getReusedSshVectorIndex(size_t ssSize) {
        int index = 0;
        if (ssSize == NEO::BindlessImageSlot::max * surfaceStateSize) {
            index = 1;
        } else {
            UNRECOVERABLE_IF(ssSize != surfaceStateSize);
        }
        return index;
    }
    bool getStateDirtyForContext(uint32_t osContextId);
    void clearStateDirtyForContext(uint32_t osContextId);

  protected:
    bool growHeap(BindlesHeapType heapType);
    bool initializeReservedMemory();
    bool isReservedMemoryModeAvailable();
    bool tryReservingMemoryForSpecialSsh(const size_t size, size_t alignment);
    std::optional<AddressRange> reserveMemoryRange(size_t size, size_t alignment, HeapIndex heapIndex);

    std::mutex mtx;

    Device *rootDevice = nullptr;
    MemoryManager *memManager = nullptr;
    GraphicsAllocation *borderColorStates = nullptr;

    std::vector<AddressRange> reservedRanges;
    std::vector<GraphicsAllocation *> ssHeapsAllocations;
    std::array<std::vector<SurfaceStateInHeapInfo>, 2> surfaceStateInHeapVectorReuse[2];
    std::unique_ptr<IndirectHeap> surfaceStateHeaps[BindlesHeapType::numHeapTypes];
    std::unique_ptr<HeapAllocator> heapFrontWindow;
    std::unique_ptr<HeapAllocator> heapRegular;

    size_t reuseSlotCountThreshold = 512;
    uint64_t reservedRangeBase = 0;
    std::bitset<64> stateCacheDirtyForContext;
    DeviceBitfield deviceBitfield;
    const size_t surfaceStateSize;

    const uint32_t rootDeviceIndex;
    uint32_t allocatePoolIndex = 0;
    uint32_t releasePoolIndex = 0;

    bool allocateFromReusePool = false;
    bool globalBindlessDsh = false;
    bool isMultiOsContextCapable = false;
    bool reservedMemoryInitialized = false;
    bool useReservedMemory = false;
    bool heaplessEnabled = false;
};

static_assert(NEO::NonCopyableAndNonMovable<BindlessHeapsHelper>);

} // namespace NEO
