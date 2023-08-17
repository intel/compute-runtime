/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace NEO {

class IndirectHeap;

class BindlessHeapsHelper {
  public:
    enum BindlesHeapType {
        SPECIAL_SSH = 0,
        GLOBAL_SSH,
        GLOBAL_DSH,
        NUM_HEAP_TYPES
    };
    BindlessHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex, DeviceBitfield deviceBitfield);
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
    void placeSSAllocationInReuseVectorOnFreeMemory(GraphicsAllocation *gfxAllocation);
    bool isGlobalDshSupported() {
        return globalBindlessDsh;
    }

    int getReusedSshVectorIndex(size_t ssSize) {
        int index = 0;
        if (ssSize == 2 * surfaceStateSize) {
            index = 1;
        } else {
            UNRECOVERABLE_IF(ssSize != surfaceStateSize);
        }
        return index;
    }

  protected:
    const size_t surfaceStateSize;
    bool growHeap(BindlesHeapType heapType);
    MemoryManager *memManager = nullptr;
    bool isMultiOsContextCapable = false;
    const uint32_t rootDeviceIndex;
    std::unique_ptr<IndirectHeap> surfaceStateHeaps[BindlesHeapType::NUM_HEAP_TYPES];
    GraphicsAllocation *borderColorStates;
    std::vector<GraphicsAllocation *> ssHeapsAllocations;
    std::vector<SurfaceStateInHeapInfo> surfaceStateInHeapVectorReuse[2];
    std::mutex mtx;
    DeviceBitfield deviceBitfield;
    bool globalBindlessDsh = false;
};
} // namespace NEO
