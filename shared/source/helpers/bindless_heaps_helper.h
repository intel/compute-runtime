/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace NEO {

class IndirectHeap;
class BindlessHeapsHelper {
  public:
    enum BindlesHeapType {
        specialSsh = 0,
        globalSsh,
        globalDsh,
        numHeapTypes
    };
    BindlessHeapsHelper(Device *rootDevice, bool isMultiOsContextCapable);
    MOCKABLE_VIRTUAL ~BindlessHeapsHelper();

    BindlessHeapsHelper(const BindlessHeapsHelper &) = delete;
    BindlessHeapsHelper &operator=(const BindlessHeapsHelper &) = delete;

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
        if (ssSize == 3 * surfaceStateSize) {
            index = 1;
        } else {
            UNRECOVERABLE_IF(ssSize != surfaceStateSize);
        }
        return index;
    }
    bool getStateDirtyForContext(uint32_t osContextId);
    void clearStateDirtyForContext(uint32_t osContextId);

  protected:
    Device *rootDevice = nullptr;
    const size_t surfaceStateSize;
    bool growHeap(BindlesHeapType heapType);
    MemoryManager *memManager = nullptr;
    bool isMultiOsContextCapable = false;
    const uint32_t rootDeviceIndex;
    std::unique_ptr<IndirectHeap> surfaceStateHeaps[BindlesHeapType::numHeapTypes];
    GraphicsAllocation *borderColorStates;
    std::vector<GraphicsAllocation *> ssHeapsAllocations;

    size_t reuseSlotCountThreshold = 512;
    uint32_t allocatePoolIndex = 0;
    uint32_t releasePoolIndex = 0;
    bool allocateFromReusePool = false;
    std::array<std::vector<SurfaceStateInHeapInfo>, 2> surfaceStateInHeapVectorReuse[2];
    std::bitset<64> stateCacheDirtyForContext;

    std::mutex mtx;
    DeviceBitfield deviceBitfield;
    bool globalBindlessDsh = false;
};
} // namespace NEO
