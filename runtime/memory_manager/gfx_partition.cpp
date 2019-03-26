/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/gfx_partition.h"

#include "runtime/helpers/ptr_math.h"

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
    const uint64_t gfxGranularity = 2 * MemoryConstants::megaByte;

    if (gpuAddressSpace == MemoryConstants::max48BitAddress) { // Full Range SVM
        // Heap base should be greater than zero and 2MB aligned
        heapInit(HeapIndex::HEAP_SVM, gfxGranularity, gfxBase - gfxGranularity);
    } else {
        // There is no SVM in LimitedRange - all memory except 4 32-bit heaps
        // goes to STANDARD and STANDARD64KB partitions
        gfxBase = 0ull;
    }

    for (auto heap : GfxPartition::heap32Names) {
        heapInit(heap, gfxBase ? gfxBase : gfxGranularity, gfxBase ? gfxHeap32Size : gfxHeap32Size - gfxGranularity);
        gfxBase += gfxHeap32Size;
    }

    uint64_t gfxStandardSize = (gfxTop - gfxBase) >> 1;

    heapInit(HeapIndex::HEAP_STANDARD, gfxBase, gfxStandardSize);
    gfxBase += gfxStandardSize;

    auto gfxBaseAligned = alignUp(gfxBase, gfxGranularity);
    heapInit(HeapIndex::HEAP_STANDARD64KB, gfxBaseAligned, gfxStandardSize - ptrDiff(gfxBaseAligned, gfxBase));
}

} // namespace NEO
