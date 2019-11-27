/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/helpers/basic_math.h"
#include "core/helpers/ptr_math.h"
#include "core/os_interface/os_memory.h"
#include "unit_tests/mocks/mock_gfx_partition.h"

#include "gtest/gtest.h"

using namespace NEO;

constexpr size_t reservedCpuAddressRangeSize = is64bit ? (6 * 4 * GB) : 0;
constexpr uint64_t sizeHeap32 = 4 * MemoryConstants::gigaByte;

void testGfxPartition(MockGfxPartition &gfxPartition, uint64_t gfxBase, uint64_t gfxTop, uint64_t svmTop) {
    if (svmTop) {
        // SVM should be initialized
        EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_SVM));
        EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::HEAP_SVM), 0ull);
        EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::HEAP_SVM), svmTop);
        EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::HEAP_SVM), svmTop - 1);
    } else {
        // Limited range
        EXPECT_FALSE(gfxPartition.heapInitialized(HeapIndex::HEAP_SVM));
    }

    for (auto heap32 : GfxPartition::heap32Names) {
        EXPECT_TRUE(gfxPartition.heapInitialized(heap32));
        EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(gfxPartition.getHeapBase(heap32)));
        EXPECT_EQ(gfxPartition.getHeapBase(heap32), gfxBase);
        EXPECT_EQ(gfxPartition.getHeapSize(heap32), sizeHeap32);
        gfxBase += sizeHeap32;
    }

    uint64_t sizeStandard = (gfxTop - gfxBase) >> 1;

    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_STANDARD));
    auto heapStandardBase = gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD);
    auto heapStandardSize = gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandardBase));
    EXPECT_EQ(heapStandardBase, gfxBase);
    EXPECT_EQ(heapStandardSize, sizeStandard);

    gfxBase += sizeStandard;
    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_STANDARD64KB));
    auto heapStandard64KbBase = gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD64KB);
    auto heapStandard64KbSize = gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD64KB);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandard64KbBase));

    EXPECT_EQ(heapStandard64KbBase, heapStandardBase + heapStandardSize);
    EXPECT_EQ(heapStandard64KbSize, heapStandardSize);
    EXPECT_EQ(heapStandard64KbBase + heapStandard64KbSize, gfxTop);
    EXPECT_EQ(gfxBase + sizeStandard, gfxTop);

    size_t sizeSmall = MemoryConstants::pageSize;
    size_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    for (auto heap : MockGfxPartition::allHeapNames) {
        if (!gfxPartition.heapInitialized(heap)) {
            EXPECT_TRUE(heap == HeapIndex::HEAP_SVM);
            continue;
        }

        EXPECT_GT(gfxPartition.getHeapMinimalAddress(heap), gfxPartition.getHeapBase(heap));
        EXPECT_EQ(gfxPartition.getHeapMinimalAddress(heap), gfxPartition.getHeapBase(heap) + GfxPartition::heapGranularity);

        auto ptrBig = gfxPartition.heapAllocate(heap, sizeBig);
        EXPECT_NE(ptrBig, 0ull);
        EXPECT_LT(gfxPartition.getHeapBase(heap), ptrBig);
        EXPECT_EQ(ptrBig, gfxPartition.getHeapBase(heap) + GfxPartition::heapGranularity);
        gfxPartition.heapFree(heap, ptrBig, sizeBig);

        auto ptrSmall = gfxPartition.heapAllocate(heap, sizeSmall);
        EXPECT_NE(ptrSmall, 0ull);
        EXPECT_LT(gfxPartition.getHeapBase(heap), ptrSmall);
        EXPECT_GT(gfxPartition.getHeapLimit(heap), ptrSmall);
        EXPECT_EQ(ptrSmall, gfxPartition.getHeapBase(heap) + gfxPartition.getHeapSize(heap) - GfxPartition::heapGranularity - sizeSmall);
        gfxPartition.heapFree(heap, ptrSmall, sizeSmall);
    }
}

TEST(GfxPartitionTest, testGfxPartitionFullRange48BitSVM) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0);

    uint64_t gfxTop = maxNBitValue(48) + 1;
    uint64_t gfxBase = MemoryConstants::maxSvmAddress + 1;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, gfxBase);
}

TEST(GfxPartitionTest, testGfxPartitionFullRange47BitSVM) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, 0);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : (uint64_t)gfxPartition.getReservedCpuAddressRange();
    uint64_t gfxTop = is32bit ? maxNBitValue(47) + 1 : gfxBase + gfxPartition.getReservedCpuAddressRangeSize();
    uint64_t svmTop = MemoryConstants::maxSvmAddress + 1;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, svmTop);
}

TEST(GfxPartitionTest, testGfxPartitionLimitedRange) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(47 - 1), reservedCpuAddressRangeSize, 0);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : 0ull;
    uint64_t gfxTop = maxNBitValue(47 - 1) + 1;
    uint64_t svmTop = gfxBase;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, svmTop);
}

TEST(GfxPartitionTest, testGfxPartitionUnsupportedRange) {
    if (is32bit) {
        GTEST_SKIP();
    }

    MockGfxPartition gfxPartition;
    EXPECT_THROW(gfxPartition.init(maxNBitValue(48 + 1), reservedCpuAddressRangeSize, 0), std::exception);
}
