/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace NEO {

constexpr size_t ssAligment = MemoryConstants::cacheLineSize;

class IndirectHeap;
class GraphicsAllocation;

struct SurfaceStateInHeapInfo {
    GraphicsAllocation *heapAllocation;
    uint64_t surfaceStateOffset;
};

class BindlessHeapsHelper {
  public:
    BindlessHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex);
    ~BindlessHeapsHelper();
    GraphicsAllocation *getHeapAllocation(size_t heapSize, size_t alignment);

    SurfaceStateInHeapInfo *allocateSSInHeap(size_t ssSize, void *ssPtr, GraphicsAllocation *surfaceAllocation);
    uint64_t getGlobalSshBase();
    void *getSpaceInSsh(size_t ssSize);
    uint32_t getDefaultBorderColorOffset();
    uint32_t getAlphaBorderColorOffset();

  protected:
    void growGlobalSSh();
    MemoryManager *memManager = nullptr;
    bool isMultiOsContextCapable = false;
    const uint32_t rootDeviceIndex;
    std::unique_ptr<IndirectHeap> specialSsh;
    std::unique_ptr<IndirectHeap> globalSsh;
    GraphicsAllocation *borderColorStates;
    std::vector<GraphicsAllocation *> ssHeapsAllocations;
    std::unordered_map<GraphicsAllocation *, std::unique_ptr<SurfaceStateInHeapInfo>> surfaceStateInHeapAllocationMap;
};
} // namespace NEO