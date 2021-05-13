/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/os_memory.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"

#include "gtest/gtest.h"

#include <mutex>

static std::string mockCpuFlags;
static void mockGetCpuFlagsFunc(std::string &cpuFlags) { cpuFlags = mockCpuFlags; }
static void (*getCpuFlagsFuncSave)(std::string &) = nullptr;

// CpuInfo is a singleton object so we have to patch it in place
class CpuInfoOverrideVirtualAddressSizeAndFlags {
  public:
    class MockCpuInfo : public CpuInfo {
      public:
        using CpuInfo::cpuFlags;
        using CpuInfo::virtualAddressSize;
    } *mockCpuInfo = reinterpret_cast<MockCpuInfo *>(const_cast<CpuInfo *>(&CpuInfo::getInstance()));

    CpuInfoOverrideVirtualAddressSizeAndFlags(uint32_t newCpuVirtualAddressSize, const char *newCpuFlags = "") {
        virtualAddressSizeSave = mockCpuInfo->getVirtualAddressSize();
        mockCpuInfo->virtualAddressSize = newCpuVirtualAddressSize;

        getCpuFlagsFuncSave = mockCpuInfo->getCpuFlagsFunc;
        mockCpuInfo->getCpuFlagsFunc = mockGetCpuFlagsFunc;
        resetCpuFlags();
        mockCpuFlags = newCpuFlags;
    }

    ~CpuInfoOverrideVirtualAddressSizeAndFlags() {
        mockCpuInfo->virtualAddressSize = virtualAddressSizeSave;
        resetCpuFlags();
        mockCpuInfo->getCpuFlagsFunc = getCpuFlagsFuncSave;
        getCpuFlagsFuncSave = nullptr;
    }

    void resetCpuFlags() {
        mockCpuFlags = "";
        mockCpuInfo->getCpuFlagsFunc(mockCpuInfo->cpuFlags);
    }

    uint32_t virtualAddressSizeSave = 0;
};

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

    constexpr uint32_t numStandardHeaps = static_cast<uint32_t>(HeapIndex::HEAP_STANDARD2MB) - static_cast<uint32_t>(HeapIndex::HEAP_STANDARD) + 1;
    constexpr uint64_t maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);

    gfxBase = alignUp(gfxBase, maxStandardHeapGranularity);
    uint64_t maxStandardHeapSize = alignDown((gfxTop - gfxBase) / numStandardHeaps, maxStandardHeapGranularity);

    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_STANDARD));
    auto heapStandardBase = gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD);
    auto heapStandardSize = gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandardBase));
    EXPECT_EQ(heapStandardBase, gfxBase);
    EXPECT_EQ(heapStandardSize, maxStandardHeapSize);

    gfxBase += maxStandardHeapSize;
    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_STANDARD64KB));
    auto heapStandard64KbBase = gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD64KB);
    auto heapStandard64KbSize = gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD64KB);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandard64KbBase));
    EXPECT_EQ(heapStandard64KbBase, heapStandardBase + heapStandardSize);
    EXPECT_EQ(heapStandard64KbSize, heapStandardSize);

    gfxBase += maxStandardHeapSize;
    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::HEAP_STANDARD2MB));
    auto heapStandard2MbBase = gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD2MB);
    auto heapStandard2MbSize = gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD2MB);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandard2MbBase));
    EXPECT_EQ(heapStandard2MbBase, heapStandard64KbBase + heapStandard64KbSize);
    EXPECT_EQ(heapStandard2MbSize, heapStandard64KbSize);

    EXPECT_LE(heapStandard2MbBase + heapStandard2MbSize, gfxTop);
    EXPECT_LE(gfxBase + maxStandardHeapSize, gfxTop);

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW));
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW));

    size_t sizeSmall = MemoryConstants::pageSize;
    size_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    for (auto heap : MockGfxPartition::allHeapNames) {
        if (!gfxPartition.heapInitialized(heap)) {
            EXPECT_TRUE(heap == HeapIndex::HEAP_SVM || heap == HeapIndex::HEAP_EXTENDED);
            continue;
        }

        const bool isInternalHeapType = heap == HeapIndex::HEAP_INTERNAL || heap == HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY;
        const auto heapGranularity = (heap == HeapIndex::HEAP_STANDARD2MB) ? GfxPartition::heapGranularity2MB : GfxPartition::heapGranularity;

        if (heap == HeapIndex::HEAP_SVM) {
            EXPECT_EQ(gfxPartition.getHeapMinimalAddress(heap), gfxPartition.getHeapBase(heap));
        } else if (isInternalHeapType) {
            EXPECT_EQ(gfxPartition.getHeapMinimalAddress(heap), gfxPartition.getHeapBase(heap) + GfxPartition::internalFrontWindowPoolSize);
        } else {
            EXPECT_GT(gfxPartition.getHeapMinimalAddress(heap), gfxPartition.getHeapBase(heap));
            EXPECT_EQ(gfxPartition.getHeapMinimalAddress(heap), gfxPartition.getHeapBase(heap) + heapGranularity);
        }

        auto ptrBig = gfxPartition.heapAllocate(heap, sizeBig);
        EXPECT_NE(ptrBig, 0ull);

        if (isInternalHeapType) {
            EXPECT_EQ(ptrBig, gfxPartition.getHeapBase(heap) + GfxPartition::internalFrontWindowPoolSize);
        } else {
            EXPECT_EQ(ptrBig, gfxPartition.getHeapBase(heap) + heapGranularity);
        }
        gfxPartition.heapFree(heap, ptrBig, sizeBig);

        auto ptrSmall = gfxPartition.heapAllocate(heap, sizeSmall);
        EXPECT_NE(ptrSmall, 0ull);

        EXPECT_LT(gfxPartition.getHeapBase(heap), ptrSmall);
        EXPECT_GT(gfxPartition.getHeapLimit(heap), ptrSmall);
        EXPECT_EQ(ptrSmall, gfxPartition.getHeapBase(heap) + gfxPartition.getHeapSize(heap) - heapGranularity - sizeSmall);

        gfxPartition.heapFree(heap, ptrSmall, sizeSmall);
    }
}

TEST(GfxPartitionTest, GivenFullRange48BitSvmWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1);

    uint64_t gfxTop = maxNBitValue(48) + 1;
    uint64_t gfxBase = MemoryConstants::maxSvmAddress + 1;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, gfxBase);
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, 0, 1);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : (uint64_t)gfxPartition.getReservedCpuAddressRange();
    uint64_t gfxTop = is32bit ? maxNBitValue(47) + 1 : gfxBase + gfxPartition.getReservedCpuAddressRangeSize();
    uint64_t svmTop = MemoryConstants::maxSvmAddress + 1;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, svmTop);
}

TEST(GfxPartitionTest, GivenLimitedRangeWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(47 - 1), reservedCpuAddressRangeSize, 0, 1);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : 0ull;
    uint64_t gfxTop = maxNBitValue(47 - 1) + 1;
    uint64_t svmTop = gfxBase;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, svmTop);
}

TEST(GfxPartitionTest, GivenUnsupportedGpuRangeThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    MockGfxPartition gfxPartition;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(48 + 1), reservedCpuAddressRangeSize, 0, 1));
}

TEST(GfxPartitionTest, GivenUnsupportedCpuRangeThenGfxPartitionIsNotInitialize) {
    if (is32bit) {
        GTEST_SKIP();
    }

    CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(48 + 1);
    MockGfxPartition gfxPartition;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1));
}

TEST(GfxPartitionTest, GivenFullRange48BitSvmHeap64KbSplitWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    uint32_t rootDeviceIndex = 3;
    size_t numRootDevices = 5;

    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, rootDeviceIndex, numRootDevices);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : maxNBitValue(48 - 1) + 1;
    uint64_t gfxTop = maxNBitValue(48) + 1;

    constexpr auto numStandardHeaps = static_cast<uint32_t>(HeapIndex::HEAP_STANDARD2MB) - static_cast<uint32_t>(HeapIndex::HEAP_STANDARD) + 1;
    constexpr auto maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);
    auto maxStandardHeapSize = alignDown((gfxTop - gfxBase - 4 * sizeHeap32) / numStandardHeaps, maxStandardHeapGranularity);
    auto heapStandard64KBSize = alignDown(maxStandardHeapSize / numRootDevices, GfxPartition::heapGranularity);

    EXPECT_EQ(heapStandard64KBSize, gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD64KB));
    EXPECT_EQ(gfxBase + 4 * sizeHeap32 + maxStandardHeapSize + rootDeviceIndex * heapStandard64KBSize, gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD64KB));
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmHeap64KbSplitWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    uint32_t rootDeviceIndex = 3;
    size_t numRootDevices = 5;

    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, rootDeviceIndex, numRootDevices);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : (uint64_t)gfxPartition.getReservedCpuAddressRange();
    uint64_t gfxTop = is32bit ? maxNBitValue(47) + 1 : gfxBase + gfxPartition.getReservedCpuAddressRangeSize();

    constexpr auto numStandardHeaps = static_cast<uint32_t>(HeapIndex::HEAP_STANDARD2MB) - static_cast<uint32_t>(HeapIndex::HEAP_STANDARD) + 1;
    constexpr auto maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);
    gfxBase = alignUp(gfxBase, maxStandardHeapGranularity);
    auto maxStandardHeapSize = alignDown((gfxTop - gfxBase - 4 * sizeHeap32) / numStandardHeaps, maxStandardHeapGranularity);
    auto heapStandard64KBSize = alignDown(maxStandardHeapSize / numRootDevices, GfxPartition::heapGranularity);

    EXPECT_EQ(heapStandard64KBSize, gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD64KB));
    EXPECT_EQ(gfxBase + 4 * sizeHeap32 + maxStandardHeapSize + rootDeviceIndex * heapStandard64KBSize, gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD64KB));
}

class MockOsMemory : public OSMemory {
  public:
    MockOsMemory() = default;
    ~MockOsMemory() { reserveCount = 0; }
    uint32_t getReserveCount() { return reserveCount; }
    struct MockOSMemoryReservedCpuAddressRange : public OSMemory::ReservedCpuAddressRange {
        MockOSMemoryReservedCpuAddressRange(void *ptr, size_t size, size_t align) {
            alignedPtr = originalPtr = ptr;
            sizeToReserve = actualReservedSize = size;
        }
    };

    OSMemory::ReservedCpuAddressRange reserveCpuAddressRange(size_t sizeToReserve, size_t alignment) override {
        return reserveCpuAddressRange(0, sizeToReserve, alignment);
    };

    OSMemory::ReservedCpuAddressRange reserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, size_t alignment) override {
        reserveCount++;
        return MockOSMemoryReservedCpuAddressRange(returnAddress, sizeToReserve, alignment);
    };

    void getMemoryMaps(MemoryMaps &outMemoryMaps) override {}

    void releaseCpuAddressRange(const OSMemory::ReservedCpuAddressRange &reservedCpuAddressRange) override{};

    void *osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve) override { return nullptr; }
    void osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) override {}

    void *returnAddress = reinterpret_cast<void *>(0x10000);
    static uint32_t reserveCount;
};

uint32_t MockOsMemory::reserveCount = 0;

TEST(GfxPartitionTest, given47bitGpuAddressSpaceWhenInitializingMultipleGfxPartitionsThenReserveCpuAddressRangeForDriverAllocationsOnlyOnce) {
    if (is32bit) {
        GTEST_SKIP();
    }

    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    std::vector<std::unique_ptr<MockGfxPartition>> gfxPartitions;
    for (int i = 0; i < 10; ++i) {
        gfxPartitions.push_back(std::make_unique<MockGfxPartition>(reservedCpuAddressRange));
        gfxPartitions[i]->osMemory.reset(new MockOsMemory);
        gfxPartitions[i]->init(maxNBitValue(47), reservedCpuAddressRangeSize, i, 10);
    }

    EXPECT_EQ(1u, static_cast<MockOsMemory *>(gfxPartitions[0]->osMemory.get())->getReserveCount());
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmAndReservedCpuRangeSizeIsZeroThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    MockGfxPartition gfxPartition;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(47), 0, 0, 1));
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmAndReturnedReservedCpuRangeIsNullThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto mockOsMemory = new MockOsMemory;
    mockOsMemory->returnAddress = nullptr;
    MockGfxPartition gfxPartition;
    gfxPartition.osMemory.reset(mockOsMemory);
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, 0, 1));
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmAndReturnedReservedCpuRangeIsNotAlignedThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto mockOsMemory = new MockOsMemory;
    mockOsMemory->returnAddress = reinterpret_cast<void *>(0x10001);
    MockGfxPartition gfxPartition;
    gfxPartition.osMemory.reset(mockOsMemory);
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, 0, 1));
}

TEST(GfxPartitionTest, givenGfxPartitionWhenInitializedThenInternalFrontWindowHeapIsAllocatedAtInternalHeapFront) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1);

    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL));
    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY));

    auto frontWindowSize = GfxPartition::internalFrontWindowPoolSize;
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW), frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW), frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW));
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW));

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL) + frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY) + frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::HEAP_INTERNAL),
              gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL) + gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL) - 1);
    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY),
              gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY) + gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY) - 1);
}

TEST(GfxPartitionTest, givenInternalFrontWindowHeapWhenAllocatingSmallOrBigChunkThenAddressFromFrontIsReturned) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1);

    const size_t sizeSmall = MemoryConstants::pageSize64k;
    const size_t sizeBig = static_cast<size_t>(gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW)) - MemoryConstants::pageSize64k;

    HeapIndex heaps[] = {HeapIndex::HEAP_INTERNAL_FRONT_WINDOW,
                         HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW};

    for (int i = 0; i < 2; i++) {
        size_t sizeToAlloc = sizeSmall;
        auto address = gfxPartition.heapAllocate(heaps[i], sizeToAlloc);

        EXPECT_EQ(gfxPartition.getHeapBase(heaps[i]), address);
        gfxPartition.heapFree(heaps[i], address, sizeToAlloc);

        sizeToAlloc = sizeBig;
        address = gfxPartition.heapAllocate(heaps[i], sizeToAlloc);

        EXPECT_EQ(gfxPartition.getHeapBase(heaps[i]), address);
        gfxPartition.heapFree(heaps[i], address, sizeToAlloc);
    }
}

TEST(GfxPartitionTest, givenInternalHeapWhenAllocatingSmallOrBigChunkThenAddressAfterFrontWindowIsReturned) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1);

    const size_t sizeSmall = MemoryConstants::pageSize64k;
    const size_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize64k;

    HeapIndex heaps[] = {HeapIndex::HEAP_INTERNAL,
                         HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY};

    for (int i = 0; i < 2; i++) {
        size_t sizeToAlloc = sizeSmall;
        auto address = gfxPartition.heapAllocate(heaps[i], sizeToAlloc);

        EXPECT_EQ(gfxPartition.getHeapLimit(heaps[i]) + 1 - sizeToAlloc - GfxPartition::heapGranularity, address);
        EXPECT_LE(gfxPartition.getHeapBase(heaps[i]) + GfxPartition::internalFrontWindowPoolSize, address);
        gfxPartition.heapFree(heaps[i], address, sizeToAlloc);

        sizeToAlloc = sizeBig;
        address = gfxPartition.heapAllocate(heaps[i], sizeToAlloc);

        EXPECT_EQ(gfxPartition.getHeapBase(heaps[i]) + GfxPartition::internalFrontWindowPoolSize, address);
        gfxPartition.heapFree(heaps[i], address, sizeToAlloc);
    }
}

using GfxPartitionTestForAllHeapTypes = ::testing::TestWithParam<HeapIndex>;

TEST_P(GfxPartitionTestForAllHeapTypes, givenHeapIndexWhenFreeGpuAddressRangeIsCalledThenFreeMemory) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1);
    const HeapIndex heapIndex = GetParam();
    const size_t allocationSize = static_cast<size_t>(gfxPartition.getHeapSize(heapIndex)) * 3 / 4;
    if (allocationSize == 0) {
        GTEST_SKIP();
    }
    if (is32bit) {
        auto it = std::find(GfxPartition::heap32Names.begin(), GfxPartition::heap32Names.end(), heapIndex);
        auto is64bitHeap = it == GfxPartition::heap32Names.end();
        if (is64bitHeap) {
            GTEST_SKIP();
        }
    }
    size_t sizeToAllocate{};

    // Allocate majority of the heap
    sizeToAllocate = allocationSize;
    auto address = gfxPartition.heapAllocate(heapIndex, sizeToAllocate);
    const size_t sizeAllocated = sizeToAllocate;
    EXPECT_NE(0ull, address);
    EXPECT_GE(sizeAllocated, allocationSize);

    // Another one cannot be allocated
    sizeToAllocate = allocationSize;
    EXPECT_EQ(0ull, gfxPartition.heapAllocate(heapIndex, sizeToAllocate));

    // Allocation is possible again after freeing
    gfxPartition.freeGpuAddressRange(address, sizeAllocated);
    sizeToAllocate = allocationSize;
    EXPECT_NE(0ull, gfxPartition.heapAllocate(heapIndex, sizeToAllocate));
}

INSTANTIATE_TEST_SUITE_P(
    GfxPartitionTestForAllHeapTypesTests,
    GfxPartitionTestForAllHeapTypes,
    ::testing::Values(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY,
                      HeapIndex::HEAP_INTERNAL,
                      HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY,
                      HeapIndex::HEAP_EXTERNAL,
                      HeapIndex::HEAP_STANDARD,
                      HeapIndex::HEAP_STANDARD64KB,
                      HeapIndex::HEAP_STANDARD2MB,
                      HeapIndex::HEAP_EXTENDED));
