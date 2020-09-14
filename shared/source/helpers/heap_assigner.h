/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
struct HardwareInfo;
struct HeapAssigner {
    HeapAssigner();
    ~HeapAssigner() = default;
    bool useExternal32BitHeap(GraphicsAllocation::AllocationType allocType);
    bool useInternal32BitHeap(GraphicsAllocation::AllocationType allocType);
    bool use32BitHeap(GraphicsAllocation::AllocationType allocType);
    HeapIndex get32BitHeapIndex(GraphicsAllocation::AllocationType allocType, bool useLocalMem, const HardwareInfo &hwInfo, bool useExternalWindow);
    static bool heapTypeWithFrontWindowPool(HeapIndex heap);

    static HeapIndex mapExternalWindowIndex(HeapIndex index);
    bool apiAllowExternalHeapForSshAndDsh = false;
};
} // namespace NEO