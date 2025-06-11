/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/memory_manager/memory_manager.h"

using namespace NEO;

class MockBindlesHeapsHelper : public BindlessHeapsHelper {
  public:
    using BaseClass = BindlessHeapsHelper;
    MockBindlesHeapsHelper(Device *rootDevice, bool isMultiOsContextCapable) : BindlessHeapsHelper(rootDevice, isMultiOsContextCapable) {
        globalSsh = surfaceStateHeaps[BindlesHeapType::globalSsh].get();
        specialSsh = surfaceStateHeaps[BindlesHeapType::specialSsh].get();
        scratchSsh = surfaceStateHeaps[BindlesHeapType::specialSsh].get();
        globalDsh = surfaceStateHeaps[BindlesHeapType::specialSsh].get();
    }

    SurfaceStateInHeapInfo allocateSSInHeap(size_t ssSize, GraphicsAllocation *surfaceAllocation, BindlesHeapType heapType) override {
        if (failAllocateSS) {
            return SurfaceStateInHeapInfo{};
        }
        return BaseClass::allocateSSInHeap(ssSize, surfaceAllocation, heapType);
    }

    using BindlesHeapType = BindlessHeapsHelper::BindlesHeapType;
    using BaseClass::allocateFromReusePool;
    using BaseClass::allocatePoolIndex;
    using BaseClass::borderColorStates;
    using BaseClass::globalBindlessDsh;
    using BaseClass::growHeap;
    using BaseClass::heapFrontWindow;
    using BaseClass::heaplessEnabled;
    using BaseClass::heapRegular;
    using BaseClass::initializeReservedMemory;
    using BaseClass::isMultiOsContextCapable;
    using BaseClass::isReservedMemoryModeAvailable;
    using BaseClass::memManager;
    using BaseClass::releasePoolIndex;
    using BaseClass::reservedMemoryInitialized;
    using BaseClass::reservedRangeBase;
    using BaseClass::reservedRanges;
    using BaseClass::reserveMemoryRange;
    using BaseClass::reuseSlotCountThreshold;
    using BaseClass::rootDeviceIndex;
    using BaseClass::ssHeapsAllocations;
    using BaseClass::stateCacheDirtyForContext;
    using BaseClass::surfaceStateHeaps;
    using BaseClass::surfaceStateInHeapVectorReuse;
    using BaseClass::surfaceStateSize;
    using BaseClass::tryReservingMemoryForSpecialSsh;
    using BaseClass::useReservedMemory;

    IndirectHeap *specialSsh;
    IndirectHeap *globalSsh;
    IndirectHeap *scratchSsh;
    IndirectHeap *globalDsh;
    bool failAllocateSS = false;
};
