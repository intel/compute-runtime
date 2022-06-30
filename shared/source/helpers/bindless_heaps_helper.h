/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/heap_helper.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace NEO {

class IndirectHeap;
class GraphicsAllocation;

struct SurfaceStateInHeapInfo {
    GraphicsAllocation *heapAllocation;
    uint64_t surfaceStateOffset;
    void *ssPtr;
};

class BindlessHeapsHelper {
  public:
    enum BindlesHeapType {
        SPECIAL_SSH = 0,
        GLOBAL_SSH,
        GLOBAL_DSH,
        SCRATCH_SSH,
        NUM_HEAP_TYPES
    };
    BindlessHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex, DeviceBitfield deviceBitfield);
    ~BindlessHeapsHelper();

    BindlessHeapsHelper(const BindlessHeapsHelper &) = delete;
    BindlessHeapsHelper &operator=(const BindlessHeapsHelper &) = delete;

    GraphicsAllocation *getHeapAllocation(size_t heapSize, size_t alignment, bool allocInFrontWindow);

    SurfaceStateInHeapInfo allocateSSInHeap(size_t ssSize, GraphicsAllocation *surfaceAllocation, BindlesHeapType heapType);
    uint64_t getGlobalHeapsBase();
    void *getSpaceInHeap(size_t ssSize, BindlesHeapType heapType);
    uint32_t getDefaultBorderColorOffset();
    uint32_t getAlphaBorderColorOffset();
    IndirectHeap *getHeap(BindlesHeapType heapType);
    void placeSSAllocationInReuseVectorOnFreeMemory(GraphicsAllocation *gfxAllocation);

  protected:
    void growHeap(BindlesHeapType heapType);
    MemoryManager *memManager = nullptr;
    bool isMultiOsContextCapable = false;
    const uint32_t rootDeviceIndex;
    std::unique_ptr<IndirectHeap> surfaceStateHeaps[BindlesHeapType::NUM_HEAP_TYPES];
    GraphicsAllocation *borderColorStates;
    std::vector<GraphicsAllocation *> ssHeapsAllocations;
    std::vector<std::unique_ptr<SurfaceStateInHeapInfo>> surfaceStateInHeapVectorReuse;
    std::unordered_map<GraphicsAllocation *, std::unique_ptr<SurfaceStateInHeapInfo>> surfaceStateInHeapAllocationMap;
    std::mutex mtx;
    DeviceBitfield deviceBitfield;
};
} // namespace NEO
