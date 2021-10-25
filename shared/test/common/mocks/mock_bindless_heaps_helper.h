/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bindless_heaps_helper.h"

using namespace NEO;

class MockBindlesHeapsHelper : public BindlessHeapsHelper {
  public:
    using BaseClass = BindlessHeapsHelper;
    MockBindlesHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex, DeviceBitfield deviceBitfield) : BaseClass(memManager, isMultiOsContextCapable, rootDeviceIndex, deviceBitfield) {
        globalSsh = surfaceStateHeaps[BindlesHeapType::GLOBAL_SSH].get();
        specialSsh = surfaceStateHeaps[BindlesHeapType::SPECIAL_SSH].get();
        scratchSsh = surfaceStateHeaps[BindlesHeapType::SPECIAL_SSH].get();
        globalDsh = surfaceStateHeaps[BindlesHeapType::SPECIAL_SSH].get();
    }
    using BindlesHeapType = BindlessHeapsHelper::BindlesHeapType;
    using BaseClass::borderColorStates;
    using BaseClass::growHeap;
    using BaseClass::isMultiOsContextCapable;
    using BaseClass::memManager;
    using BaseClass::rootDeviceIndex;
    using BaseClass::ssHeapsAllocations;
    using BaseClass::surfaceStateHeaps;
    using BaseClass::surfaceStateInHeapAllocationMap;
    using BaseClass::surfaceStateInHeapVectorReuse;

    IndirectHeap *specialSsh;
    IndirectHeap *globalSsh;
    IndirectHeap *scratchSsh;
    IndirectHeap *globalDsh;
};
