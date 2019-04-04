/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/gfx_partition.h"

#include "runtime/helpers/aligned_memory.h"

namespace NEO {

const std::array<HeapIndex, 4> GfxPartition::heap32Names{{HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY,
                                                          HeapIndex::HEAP_INTERNAL,
                                                          HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY,
                                                          HeapIndex::HEAP_EXTERNAL}};

void GfxPartition::init(uint64_t gpuAddressSpace) {

    // 1) Full Range SVM gfx layout:
    //
    //                   SVM                  H0   H1   H2   H3      STANDARD      STANDARD64K
    //   |__________________________________|____|____|____|____|________________|______________|
    //   |                                  |    |    |    |    |                |              |
    //   |                                gfxBase                                             gfxTop
    //  0x0                     0x0000800000000000/0x10000000 for 32 bit              0x0000FFFFFFFFFFFFFFFF
    //
    // 2) Limited Range gfx layout (no SVM):
    //
    //     H0   H1   H2   H3        STANDARD          STANDARD64K
    //   |____|____|____|____|____________________|__________________|
    //   |    |    |    |    |                    |                  |
    // gfxBase                                                    gfxTop
    //  0x0                                                    0xFFF...FFF < 48 bit

    uint64_t gfxTop = gpuAddressSpace + 1;
    uint64_t gfxBase = is64bit ? MemoryConstants::max64BitAppAddress + 1 : MemoryConstants::max32BitAddress + 1;
    const uint64_t gfxHeap32Size = 4 * MemoryConstants::gigaByte;

    if (gpuAddressSpace < MemoryConstants::max48BitAddress) {
        gfxBase = 0ull;
    }

    heapInit(HeapIndex::HEAP_SVM, 0ull, gfxBase);

    for (auto heap : GfxPartition::heap32Names) {
        heapInit(heap, gfxBase, gfxHeap32Size);
        gfxBase += gfxHeap32Size;
    }

    uint64_t gfxStandardSize = alignDown((gfxTop - gfxBase) >> 1, heapGranularity);

    heapInit(HeapIndex::HEAP_STANDARD, gfxBase, gfxStandardSize);
    gfxBase += gfxStandardSize;

    heapInit(HeapIndex::HEAP_STANDARD64KB, gfxBase, gfxStandardSize);
}

} // namespace NEO
