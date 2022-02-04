/*
 * Copyright (C) 2020-2022 Intel Corporation
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
    bool useExternal32BitHeap(AllocationType allocType);
    bool useInternal32BitHeap(AllocationType allocType);
    bool use32BitHeap(AllocationType allocType);
    HeapIndex get32BitHeapIndex(AllocationType allocType, bool useLocalMem, const HardwareInfo &hwInfo, bool useFrontWindow);
    static bool heapTypeExternalWithFrontWindowPool(HeapIndex heap);
    static bool isInternalHeap(HeapIndex heap);

    static HeapIndex mapExternalWindowIndex(HeapIndex index);
    static HeapIndex mapInternalWindowIndex(HeapIndex index);
    bool apiAllowExternalHeapForSshAndDsh = false;
};
} // namespace NEO