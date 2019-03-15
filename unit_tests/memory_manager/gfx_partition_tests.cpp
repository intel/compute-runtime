/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/mocks/mock_gfx_partition.h"

#include "gtest/gtest.h"

using namespace OCLRT;

void testGfxPartition(uint64_t gpuAddressSpace) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(gpuAddressSpace);

    uint64_t gfxTop = gpuAddressSpace + 1;
    uint64_t gfxBase = is64bit ? MemoryConstants::max64BitAppAddress + 1 : MemoryConstants::max32BitAddress + 1;
    const uint64_t sizeHeap32 = 4 * MemoryConstants::gigaByte;
    const uint64_t gfxGranularity = 2 * MemoryConstants::megaByte;

    if (MemoryConstants::max48BitAddress == gpuAddressSpace) {
        // Full range SVM
        EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_SVM));
        EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::HEAP_SVM), gfxGranularity);
        EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::HEAP_SVM), gfxBase - gfxGranularity);
    } else {
        // Limited range
        EXPECT_FALSE(gfxPartition.heapInitialized(HeapIndex::HEAP_SVM));
        gfxBase = 0ull;
    }

    for (auto heap32 : GfxPartition::heap32Names) {
        EXPECT_TRUE(gfxPartition.heapInitialized(heap32));
        EXPECT_TRUE(isAligned<gfxGranularity>(gfxPartition.getHeapBase(heap32)));
        EXPECT_EQ(gfxPartition.getHeapBase(heap32), gfxBase ? gfxBase : gfxGranularity);
        EXPECT_EQ(gfxPartition.getHeapSize(heap32), gfxBase ? sizeHeap32 : sizeHeap32 - gfxGranularity);
        gfxBase += sizeHeap32;
    }

    uint64_t sizeStandard = (gfxTop - gfxBase) >> 1;

    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_STANDARD));
    auto heapStandardBase = gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD);
    auto heapStandardSize = gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD);
    EXPECT_TRUE(isAligned<gfxGranularity>(heapStandardBase));
    EXPECT_EQ(heapStandardBase, gfxBase);
    EXPECT_EQ(heapStandardSize, sizeStandard);

    gfxBase += sizeStandard;
    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_STANDARD64KB));
    auto heapStandard64KbBase = gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD64KB);
    auto heapStandard64KbSize = gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD64KB);
    EXPECT_TRUE(isAligned<gfxGranularity>(heapStandard64KbBase));

    uint64_t heapStandard64KbBaseOffset = 0;
    if (gfxBase != heapStandard64KbBase) {
        EXPECT_TRUE(gfxBase < heapStandard64KbBase);
        heapStandard64KbBaseOffset = ptrDiff(heapStandard64KbBase, gfxBase);
    }

    EXPECT_EQ(heapStandard64KbBase, heapStandardBase + heapStandardSize + heapStandard64KbBaseOffset);
    EXPECT_EQ(heapStandard64KbSize, heapStandardSize - heapStandard64KbBaseOffset);
    EXPECT_EQ(heapStandard64KbBase + heapStandard64KbSize, gfxTop);
    EXPECT_EQ(gfxBase + sizeStandard, gfxTop);

    size_t sizeSmall = MemoryConstants::pageSize;
    size_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    for (auto heap : MockGfxPartition::allHeapNames) {
        if (!gfxPartition.heapInitialized(heap)) {
            EXPECT_TRUE(heap == HeapIndex::HEAP_SVM);
            continue;
        }

        auto ptrBig = gfxPartition.heapAllocate(heap, sizeBig);
        EXPECT_NE(ptrBig, 0ull);
        EXPECT_EQ(ptrBig, gfxPartition.getHeapBase(heap));
        gfxPartition.heapFree(heap, ptrBig, sizeBig);

        auto ptrSmall = gfxPartition.heapAllocate(heap, sizeSmall);
        EXPECT_NE(ptrSmall, 0ull);
        EXPECT_EQ(ptrSmall, gfxPartition.getHeapBase(heap) + gfxPartition.getHeapSize(heap) - sizeSmall);
        gfxPartition.heapFree(heap, ptrSmall, sizeSmall);
    }
}

TEST(GfxPartitionTest, testGfxPartitionFullRangeSVM) {
    testGfxPartition(MemoryConstants::max48BitAddress);
}

TEST(GfxPartitionTest, testGfxPartitionLimitedRange) {
    testGfxPartition(maxNBitValue<48 - 1>);
}
