/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/os_memory.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"

#include "gtest/gtest.h"

#include <mutex>

namespace NEO {
namespace SysCalls {
extern bool mmapAllowExtendedPointers;
}
} // namespace NEO

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

constexpr size_t reservedCpuAddressRangeSize = static_cast<size_t>(is64bit ? (6 * 4 * MemoryConstants::gigaByte) : 0);
constexpr uint64_t sizeHeap32 = 4 * MemoryConstants::gigaByte;

void testGfxPartition(MockGfxPartition &gfxPartition, uint64_t gfxBase, uint64_t gfxTop, uint64_t svmTop) {
    if (svmTop) {
        // SVM should be initialized
        EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::heapSvm));
        EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::heapSvm), 0ull);
        EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::heapSvm), svmTop);
        EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::heapSvm), svmTop - 1);
    } else {
        // Limited range
        EXPECT_FALSE(gfxPartition.heapInitialized(HeapIndex::heapSvm));
    }

    for (auto heap32 : GfxPartition::heap32Names) {
        EXPECT_TRUE(gfxPartition.heapInitialized(heap32));
        EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(gfxPartition.getHeapBase(heap32)));
        EXPECT_EQ(gfxPartition.getHeapBase(heap32), gfxBase);
        EXPECT_EQ(gfxPartition.getHeapSize(heap32), sizeHeap32);
        gfxBase += sizeHeap32;
    }

    constexpr uint32_t numStandardHeaps = static_cast<uint32_t>(HeapIndex::heapStandard2MB) - static_cast<uint32_t>(HeapIndex::heapStandard) + 1;
    constexpr uint64_t maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);

    gfxBase = alignUp(gfxBase, maxStandardHeapGranularity);
    uint64_t maxStandardHeapSize = alignDown((gfxTop - gfxBase) / numStandardHeaps, maxStandardHeapGranularity);

    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::heapStandard));
    auto heapStandardBase = gfxPartition.getHeapBase(HeapIndex::heapStandard);
    auto heapStandardSize = gfxPartition.getHeapSize(HeapIndex::heapStandard);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandardBase));
    EXPECT_EQ(heapStandardBase, gfxBase);
    EXPECT_EQ(heapStandardSize, maxStandardHeapSize);

    gfxBase += maxStandardHeapSize;
    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::heapStandard64KB));
    auto heapStandard64KbBase = gfxPartition.getHeapBase(HeapIndex::heapStandard64KB);
    auto heapStandard64KbSize = gfxPartition.getHeapSize(HeapIndex::heapStandard64KB);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandard64KbBase));
    EXPECT_EQ(heapStandard64KbBase, heapStandardBase + heapStandardSize);
    EXPECT_EQ(heapStandard64KbSize, heapStandardSize);

    gfxBase += maxStandardHeapSize;
    EXPECT_TRUE(gfxPartition.heapInitialized(HeapIndex::heapStandard2MB));
    auto heapStandard2MbBase = gfxPartition.getHeapBase(HeapIndex::heapStandard2MB);
    auto heapStandard2MbSize = gfxPartition.getHeapSize(HeapIndex::heapStandard2MB);
    EXPECT_TRUE(isAligned<GfxPartition::heapGranularity>(heapStandard2MbBase));
    EXPECT_EQ(heapStandard2MbBase, heapStandard64KbBase + heapStandard64KbSize);
    EXPECT_EQ(heapStandard2MbSize, heapStandard64KbSize);

    EXPECT_LE(heapStandard2MbBase + heapStandard2MbSize, gfxTop);
    EXPECT_LE(gfxBase + maxStandardHeapSize, gfxTop);

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalFrontWindow));
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalDeviceFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceFrontWindow));

    size_t sizeSmall = MemoryConstants::pageSize;
    size_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    for (auto heap : MockGfxPartition::allHeapNames) {
        if (!gfxPartition.heapInitialized(heap)) {
            EXPECT_TRUE(heap == HeapIndex::heapSvm || heap == HeapIndex::heapExtended);
            continue;
        }

        const bool isInternalHeapType = heap == HeapIndex::heapInternal || heap == HeapIndex::heapInternalDeviceMemory;
        const auto heapGranularity = (heap == HeapIndex::heapStandard2MB) ? GfxPartition::heapGranularity2MB : GfxPartition::heapGranularity;

        if (heap == HeapIndex::heapSvm) {
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
    uint64_t gfxTop = maxNBitValue(48) + 1;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop);

    uint64_t gfxBase = MemoryConstants::maxSvmAddress + 1;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, gfxBase);
}

TEST(GfxPartitionTest, GivenRange48BitWithoutPageWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(48) + 1 - MemoryConstants::pageSize;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop);

    uint64_t gfxBase = MemoryConstants::maxSvmAddress + 1;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, gfxBase);
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, 0, 1, false, 0u, maxNBitValue(47) + 1);

    uint64_t svmTop = MemoryConstants::maxSvmAddress + 1;
    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : (uint64_t)gfxPartition.getReservedCpuAddressRange();
    uint64_t gfxTop = is32bit ? maxNBitValue(47) + 1 : gfxBase + gfxPartition.getReservedCpuAddressRangeSize();

    testGfxPartition(gfxPartition, gfxBase, gfxTop, svmTop);
}

TEST(GfxPartitionTest, GivenLimitedRangeWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(47 - 1) + 1;
    gfxPartition.init(maxNBitValue(47 - 1), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : 0ull;
    uint64_t svmTop = gfxBase;

    testGfxPartition(gfxPartition, gfxBase, gfxTop, svmTop);
}

TEST(GfxPartitionTest, GivenUnsupportedGpuRangeThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(48) + 1;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(48 + 1), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop));
}

TEST(GfxPartitionTest, GivenUnsupportedCpuRangeThenGfxPartitionIsNotInitialize) {
    if (is32bit) {
        GTEST_SKIP();
    }

    CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(48 + 1);
    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(48) + 1;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop));
}

TEST(GfxPartitionTest, GivenFullRange48BitSvmHeap64KbSplitWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    uint32_t rootDeviceIndex = 3;
    size_t numRootDevices = 5;

    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(48) + 1;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, rootDeviceIndex, numRootDevices, false, 0u, gfxTop);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : maxNBitValue(48 - 1) + 1;

    constexpr auto numStandardHeaps = static_cast<uint32_t>(HeapIndex::heapStandard2MB) - static_cast<uint32_t>(HeapIndex::heapStandard) + 1;
    constexpr auto maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);
    auto maxStandardHeapSize = alignDown((gfxTop - gfxBase - 4 * sizeHeap32) / numStandardHeaps, maxStandardHeapGranularity);
    auto heapStandard64KBSize = alignDown(maxStandardHeapSize / numRootDevices, GfxPartition::heapGranularity);

    EXPECT_EQ(heapStandard64KBSize, gfxPartition.getHeapSize(HeapIndex::heapStandard64KB));
    EXPECT_EQ(gfxBase + 4 * sizeHeap32 + maxStandardHeapSize + rootDeviceIndex * heapStandard64KBSize, gfxPartition.getHeapBase(HeapIndex::heapStandard64KB));
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmHeap64KbSplitWhenTestingGfxPartitionThenAllExpectationsAreMet) {
    uint32_t rootDeviceIndex = 3;
    size_t numRootDevices = 5;

    MockGfxPartition gfxPartition;
    gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, rootDeviceIndex, numRootDevices, false, 0u, maxNBitValue(47) + 1);

    uint64_t gfxBase = is32bit ? MemoryConstants::maxSvmAddress + 1 : (uint64_t)gfxPartition.getReservedCpuAddressRange();
    uint64_t gfxTop = is32bit ? maxNBitValue(47) + 1 : gfxBase + gfxPartition.getReservedCpuAddressRangeSize();

    constexpr auto numStandardHeaps = static_cast<uint32_t>(HeapIndex::heapStandard2MB) - static_cast<uint32_t>(HeapIndex::heapStandard) + 1;
    constexpr auto maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);
    gfxBase = alignUp(gfxBase, maxStandardHeapGranularity);
    auto maxStandardHeapSize = alignDown((gfxTop - gfxBase - 4 * sizeHeap32) / numStandardHeaps, maxStandardHeapGranularity);
    auto heapStandard64KBSize = alignDown(maxStandardHeapSize / numRootDevices, GfxPartition::heapGranularity);

    EXPECT_EQ(heapStandard64KBSize, gfxPartition.getHeapSize(HeapIndex::heapStandard64KB));
    EXPECT_EQ(gfxBase + 4 * sizeHeap32 + maxStandardHeapSize + rootDeviceIndex * heapStandard64KBSize, gfxPartition.getHeapBase(HeapIndex::heapStandard64KB));
}

class MockOsMemory : public OSMemory {
  public:
    MockOsMemory() = default;
    ~MockOsMemory() override { reserveCount = 0; }
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

    void *osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, bool topDownHint) override { return nullptr; }
    void osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) override {}

    void *returnAddress = reinterpret_cast<void *>(0x10000);
    static uint32_t reserveCount;
};

uint32_t MockOsMemory::reserveCount = 0;

TEST(GfxPartitionTest, given47bitGpuAddressSpaceWhenInitializingMultipleGfxPartitionsThenReserveCpuAddressRangeForDriverAllocationsOnlyOnce) {
    if (is32bit) {
        GTEST_SKIP();
    }

    uint64_t gfxTop = maxNBitValue(47) + 1;
    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    std::vector<std::unique_ptr<MockGfxPartition>> gfxPartitions;
    for (int i = 0; i < 10; ++i) {
        gfxPartitions.push_back(std::make_unique<MockGfxPartition>(reservedCpuAddressRange));
        gfxPartitions[i]->osMemory.reset(new MockOsMemory);
        gfxPartitions[i]->init(maxNBitValue(47), reservedCpuAddressRangeSize, i, 10, false, 0u, gfxTop);
    }

    EXPECT_EQ(1u, static_cast<MockOsMemory *>(gfxPartitions[0]->osMemory.get())->getReserveCount());
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmAndReservedCpuRangeSizeIsZeroThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(47) + 1;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(47), 0, 0, 1, false, 0u, gfxTop));
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmAndReturnedReservedCpuRangeIsNullThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto mockOsMemory = new MockOsMemory;
    mockOsMemory->returnAddress = nullptr;
    MockGfxPartition gfxPartition;
    gfxPartition.osMemory.reset(mockOsMemory);
    uint64_t gfxTop = maxNBitValue(47) + 1;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop));
}

TEST(GfxPartitionTest, GivenFullRange47BitSvmAndReturnedReservedCpuRangeIsNotAlignedThenGfxPartitionIsNotInitialized) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto mockOsMemory = new MockOsMemory;
    mockOsMemory->returnAddress = reinterpret_cast<void *>(0x10001);
    MockGfxPartition gfxPartition;
    gfxPartition.osMemory.reset(mockOsMemory);
    uint64_t gfxTop = maxNBitValue(47) + 1;
    EXPECT_FALSE(gfxPartition.init(maxNBitValue(47), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop));
}

TEST(GfxPartitionTest, givenGfxPartitionWhenInitializedThenInternalFrontWindowHeapIsAllocatedAtInternalHeapFront) {
    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(48) + 1;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop);

    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::heapInternalFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternal));
    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceMemory));

    auto frontWindowSize = GfxPartition::internalFrontWindowPoolSize;
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::heapInternalFrontWindow), frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::heapInternalDeviceFrontWindow), frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalFrontWindow));
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalDeviceFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceFrontWindow));

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternal), gfxPartition.getHeapBase(HeapIndex::heapInternal) + frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalDeviceMemory), gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceMemory) + frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::heapInternal),
              gfxPartition.getHeapBase(HeapIndex::heapInternal) + gfxPartition.getHeapSize(HeapIndex::heapInternal) - 1);
    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::heapInternalDeviceMemory),
              gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceMemory) + gfxPartition.getHeapSize(HeapIndex::heapInternalDeviceMemory) - 1);
}

TEST(GfxPartitionTest, givenInternalFrontWindowHeapWhenAllocatingSmallOrBigChunkThenAddressFromFrontIsReturned) {
    MockGfxPartition gfxPartition;
    uint64_t gfxTop = maxNBitValue(48) + 1;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop);

    const size_t sizeSmall = MemoryConstants::pageSize64k;
    const size_t sizeBig = static_cast<size_t>(gfxPartition.getHeapSize(HeapIndex::heapInternalFrontWindow)) - MemoryConstants::pageSize64k;

    HeapIndex heaps[] = {HeapIndex::heapInternalFrontWindow,
                         HeapIndex::heapInternalDeviceFrontWindow};

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
    uint64_t gfxTop = maxNBitValue(48) + 1;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop);

    const size_t sizeSmall = MemoryConstants::pageSize64k;
    const size_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize64k;

    HeapIndex heaps[] = {HeapIndex::heapInternal,
                         HeapIndex::heapInternalDeviceMemory};

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

    uint64_t gfxTop = maxNBitValue(48) + 1;
    gfxPartition.init(maxNBitValue(48), reservedCpuAddressRangeSize, 0, 1, false, 0u, gfxTop);
    gfxPartition.callBasefreeGpuAddressRange = true;
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
    ::testing::Values(HeapIndex::heapInternalDeviceMemory,
                      HeapIndex::heapInternal,
                      HeapIndex::heapExternalDeviceMemory,
                      HeapIndex::heapExternal,
                      HeapIndex::heapStandard,
                      HeapIndex::heapStandard64KB,
                      HeapIndex::heapStandard2MB,
                      HeapIndex::heapExtended));

struct GfxPartitionOn57bTest : public ::testing::Test {
  public:
    class MockOsMemory : public OSMemory {
      public:
        MockOsMemory() = default;
        ~MockOsMemory() override { reserveCount = 0; }
        uint32_t getReserveCount() { return reserveCount; }
        struct MockOSMemoryReservedCpuAddressRange : public OSMemory::ReservedCpuAddressRange {
            MockOSMemoryReservedCpuAddressRange(void *ptr, size_t size, size_t align) {
                alignedPtr = originalPtr = ptr;
                sizeToReserve = actualReservedSize = size;
            }
        };

        OSMemory::ReservedCpuAddressRange reserveCpuAddressRange(size_t sizeToReserve, size_t alignment) override {
            return reserveCpuAddressRange(validReturnAddress, sizeToReserve, alignment);
        };

        OSMemory::ReservedCpuAddressRange reserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, size_t alignment) override {
            if (forceParseMemoryMaps) {
                forceParseMemoryMaps = false;
                return MockOSMemoryReservedCpuAddressRange(nullptr, sizeToReserve, alignment);
            }

            reservationSizes.push_back(static_cast<uint64_t>(sizeToReserve));
            reserveCount++;

            if (returnInvalidAddressFirst || returnInvalidAddressAlways) {
                returnInvalidAddressFirst = false;
                return MockOSMemoryReservedCpuAddressRange(invalidReturnAddress, sizeToReserve, alignment);
            }

            validReturnAddress = baseAddress;
            return MockOSMemoryReservedCpuAddressRange(validReturnAddress, sizeToReserve, alignment);
        };

        void releaseCpuAddressRange(const OSMemory::ReservedCpuAddressRange &reservedCpuAddressRange) override {
            freeCounter++;
        };

        void getMemoryMaps(MemoryMaps &outMemoryMaps) override {
            getMemoryMapsCalled = true;
            outMemoryMaps = memoryMaps;
        }

        void *osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, bool topDownHint) override { return nullptr; }
        void osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) override {}

        void *validReturnAddress = reinterpret_cast<void *>(0x10000);
        void *invalidReturnAddress = reinterpret_cast<void *>(maxNBitValue(47));
        bool returnInvalidAddressFirst = false;
        bool returnInvalidAddressAlways = false;

        OSMemory::MemoryMaps memoryMaps;
        std::vector<uint64_t> reservationSizes;
        static uint32_t reserveCount;
        uint32_t freeCounter = 0;
        bool getMemoryMapsCalled = false;
        bool forceParseMemoryMaps = false;
    };

    void verifyHeaps(uint64_t gfxBase, uint64_t gfxTop, uint64_t svmLimit, bool expectHeapExtendedInitialized) {
        EXPECT_EQ(0u, gfxPartition->getHeapBase(HeapIndex::heapSvm));
        EXPECT_EQ(svmLimit, gfxPartition->getHeapLimit(HeapIndex::heapSvm));

        constexpr uint64_t gfxHeap32Size = 4 * MemoryConstants::gigaByte;
        for (auto heap : GfxPartition::heap32Names) {
            EXPECT_EQ(gfxBase, gfxPartition->getHeapBase(heap));
            EXPECT_EQ(gfxBase + gfxHeap32Size - 1, gfxPartition->getHeapLimit(heap));

            gfxBase += gfxHeap32Size;
        }

        constexpr uint32_t numStandardHeaps = static_cast<uint32_t>(HeapIndex::heapStandard2MB) - static_cast<uint32_t>(HeapIndex::heapStandard) + 1;
        constexpr uint64_t maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);

        gfxBase = alignUp(gfxBase, maxStandardHeapGranularity);
        uint64_t maxStandardHeapSize = alignDown((gfxTop - gfxBase) / numStandardHeaps, maxStandardHeapGranularity);
        uint64_t maxStandard64HeapSize = maxStandardHeapSize;

        if (expectHeapExtendedInitialized) {
            maxStandardHeapSize *= 2;
            maxStandard64HeapSize /= 2;
        }

        EXPECT_EQ(gfxBase, gfxPartition->getHeapBase(HeapIndex::heapStandard));
        EXPECT_EQ(gfxBase + maxStandardHeapSize - 1, gfxPartition->getHeapLimit(HeapIndex::heapStandard));

        gfxBase += maxStandardHeapSize;

        EXPECT_EQ(gfxBase, gfxPartition->getHeapBase(HeapIndex::heapStandard64KB));
        EXPECT_EQ(gfxBase + maxStandard64HeapSize - 1, gfxPartition->getHeapLimit(HeapIndex::heapStandard64KB));

        if (expectHeapExtendedInitialized) {
            EXPECT_TRUE(gfxPartition->heapInitialized(HeapIndex::heapExtended));
        } else {
            EXPECT_FALSE(gfxPartition->heapInitialized(HeapIndex::heapExtended));
        }
    }

    void verifyReservedHeaps(uint64_t gfxBase, uint64_t gfxTop, uint32_t gpuAddressSpace) {
        auto mockOsMemory = static_cast<MockOsMemory *>(gfxPartition->osMemory.get());

        auto reservedSpaceStart = castToUint64(mockOsMemory->validReturnAddress);
        auto reservedSpaceEnd = reservedSpaceStart + mockOsMemory->reservationSizes.back();

        EXPECT_EQ(gfxBase, reservedSpaceStart);
        EXPECT_EQ(gfxTop, reservedSpaceEnd);

        uint64_t svmLimit = (gpuAddressSpace == 57) ? maxNBitValue(57 - 1) : maxNBitValue(48);
        verifyHeaps(reservedSpaceStart, reservedSpaceEnd, svmLimit, gpuAddressSpace == 57);
    }

    void resetGfxPartition() {
        mockOsMemory = new MockOsMemory();

        gfxPartition = std::make_unique<MockGfxPartition>();
        gfxPartition->osMemory.reset(mockOsMemory);
    }

    MockOsMemory *mockOsMemory = nullptr;
    std::unique_ptr<MockGfxPartition> gfxPartition;
};

uint32_t GfxPartitionOn57bTest::MockOsMemory::reserveCount = 0;

TEST_F(GfxPartitionOn57bTest, given48bitCpuAddressWidthWhenInitializingGfxPartitionThenDoNotReserve48bitSpaceForDriverAllocations) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto gpuAddressSpace = 48;

    // 48 bit CPU VA - do not reserve CPU address range, use [0x800000000000-0xFFFFFFFFFFFF]
    CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(48);

    resetGfxPartition();
    uint64_t gfxTop = maxNBitValue(gpuAddressSpace) + 1;
    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(0u, mockOsMemory->freeCounter);
    EXPECT_EQ(0u, mockOsMemory->reservationSizes.size());
    /* init HEAP_EXTENDED only on 57 bit GPU */
    verifyHeaps(0x800000000000, 0x1000000000000, 0x7FFFFFFFFFFF, gpuAddressSpace == 57);
}

TEST_F(GfxPartitionOn57bTest, given57bitCpuAddressWidthAndLa57IsNotPresentWhenInitializingGfxPartitionThenDoNotReserve48bitSpaceForDriverAllocations) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto gpuAddressSpace = 57;

    // 57 bit CPU VA, la57 is not present - do not reserve CPU address range, use [0x800000000000-0xFFFFFFFFFFFF]
    CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(57);

    resetGfxPartition();

    uint64_t gfxTop = maxNBitValue(gpuAddressSpace) + 1;
    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(0u, mockOsMemory->freeCounter);
    EXPECT_EQ(0u, mockOsMemory->reservationSizes.size());
    /* init HEAP_EXTENDED only on 57 bit GPU */
    verifyHeaps(0x800000000000, 0x1000000000000, 0x7FFFFFFFFFFF, gpuAddressSpace == 57);
}

TEST_F(GfxPartitionOn57bTest, given57bitCpuAddressWidthAndLa57IsPresentWhenInitializingGfxPartitionThenReserve48bitSpaceForDriverAllocations) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto gpuAddressSpace = 48;

    // 57 bit CPU VA, la57 flag is present  - reserve high or low CPU address range depending of memory maps
    CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(57, "la57");

    constexpr uint64_t reservedHighSize = MemoryConstants::teraByte;

    // Success on first reserve
    resetGfxPartition();

    uint64_t gfxTop = maxNBitValue(gpuAddressSpace) + 1;
    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(0u, mockOsMemory->freeCounter);
    EXPECT_EQ(1u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_FALSE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    // nullptr returned first
    resetGfxPartition();

    mockOsMemory->invalidReturnAddress = nullptr;
    mockOsMemory->returnInvalidAddressFirst = true;

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(2u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[1]);
    EXPECT_EQ(0u, mockOsMemory->freeCounter);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    // start in low 48 bits
    resetGfxPartition();

    mockOsMemory->invalidReturnAddress = reinterpret_cast<void *>(maxNBitValue(47) - reservedHighSize + 1);
    mockOsMemory->returnInvalidAddressFirst = true;

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(2u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[1]);
    EXPECT_EQ(1u, mockOsMemory->freeCounter);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    // start in range, but end above 48b
    resetGfxPartition();

    mockOsMemory->invalidReturnAddress = reinterpret_cast<void *>(maxNBitValue(48) - reservedHighSize + 1);
    mockOsMemory->returnInvalidAddressFirst = true;

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(2u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[1]);
    EXPECT_EQ(1u, mockOsMemory->freeCounter);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    // start out of bounds
    resetGfxPartition();

    mockOsMemory->invalidReturnAddress = reinterpret_cast<void *>(maxNBitValue(48) + 1);
    mockOsMemory->returnInvalidAddressFirst = true;

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(2u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[1]);
    EXPECT_EQ(1u, mockOsMemory->freeCounter);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    // Empty memory map
    resetGfxPartition();

    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {};

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(0u, mockOsMemory->freeCounter);
    EXPECT_EQ(1u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(MemoryConstants::teraByte, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reinterpret_cast<void *>(maxNBitValue(47) + 1), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    // Something reserved in low memory only
    resetGfxPartition();

    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {{0x7ffff7ff3000ull, 0x7ffff7ffb000ull}, {0x7ffff7ffc000ull, 0x7ffff7ffd000ull}, {0x7ffff7ffd000ull, 0x7ffff7ffe000ull}, {0x7ffff7ffe000ull, 0x7ffff7fff000ull}, {0x7ffffffde000ull, 0x7ffffffff000ull}};

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(1u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    resetGfxPartition();

    // Something reserved in low memory and in high memory above 48 bit
    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {{0x7ffff7ff3000ull, 0x7ffff7ffb000ull}, {0x7ffff7ffc000ull, 0x7ffff7ffd000ull}, {0x7ffff7ffd000ull, 0x7ffff7ffe000ull}, {0x7ffff7ffe000ull, 0x7ffff7fff000ull}, {0x7ffffffde000ull, 0x7ffffffff000ull}, {0xffffffffff600000ull, 0xffffffffff601000ull}};

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(1u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    resetGfxPartition();

    // Something reserved in low memory, at the end of 48 high bits and in high memory above 48 bit
    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {{0x7ffff7ff3000ull, 0x7ffff7ffb000ull}, {0x7ffff7ffc000ull, 0x7ffff7ffd000ull}, {0x7ffff7ffd000ull, 0x7ffff7ffe000ull}, {0x7ffff7ffe000ull, 0x7ffff7fff000ull}, {0x7ffffffde000ull, 0x7ffffffff000ull}, {0xffffff600000ull, 0xffffff601000ull}, {0xffffffffff600000ull, 0xffffffffff601000ull}};

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(1u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000000000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000000000, 0x800000000000 + reservedHighSize, gpuAddressSpace);

    // Something reserved in low memory, at the beginning of high 48 bits, end of high 48 bits and in high memory above 48 bit
    resetGfxPartition();

    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {{0x7ffff7ff3000ull, 0x7ffff7ffb000ull}, {0x7ffff7ffc000ull, 0x7ffff7ffd000ull}, {0x7ffff7ffd000ull, 0x7ffff7ffe000ull}, {0x7ffff7ffe000ull, 0x7ffff7fff000ull}, {0x80000013e000ull, 0x800000141000ull}, {0x800000141000ull, 0x800000142000ull}, {0x7ffffffde000ull, 0x7ffffffff000ull}, {0xffffff600000ull, 0xffffff601000ull}, {0xffffffffff600000ull, 0xffffffffff601000ull}};

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(1u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedHighSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reinterpret_cast<void *>(0x800000142000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x800000142000, 0x800000142000 + reservedHighSize, gpuAddressSpace);

    // No space in high 48 bits, allocate in low space, first try is OK
    resetGfxPartition();

    constexpr uint64_t reservedLowSize = 256 * MemoryConstants::gigaByte;
    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {{0x7ffff7ff3000ull, 0x7ffff7ffb000ull}, {0x7ffff7ffc000ull, 0x7ffff7ffd000ull}, {0x7ffff7ffd000ull, 0x7ffff7ffe000ull}, {0x7ffff7ffe000ull, 0x7ffff7fff000ull}, {0x800000000000ull, 0x1000000000000ull}, {0xffffffffff600000ull, 0xffffffffff601000ull}};

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(1u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedLowSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reinterpret_cast<void *>(0x10000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x10000, 0x10000 + reservedLowSize, gpuAddressSpace);

    // No space in high 48 bits, allocate in low space, first try is failed, second is OK
    resetGfxPartition();

    constexpr uint64_t reservedLowSize2 = alignDown(static_cast<uint64_t>(reservedLowSize * 0.9), MemoryConstants::pageSize64k);
    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {{0x7ffff7ff3000ull, 0x7ffff7ffb000ull}, {0x7ffff7ffc000ull, 0x7ffff7ffd000ull}, {0x7ffff7ffd000ull, 0x7ffff7ffe000ull}, {0x7ffff7ffe000ull, 0x7ffff7fff000ull}, {0x800000000000ull, 0x1000000000000ull}, {0xffffffffff600000ull, 0xffffffffff601000ull}};

    mockOsMemory->invalidReturnAddress = nullptr;
    mockOsMemory->returnInvalidAddressFirst = true;

    EXPECT_TRUE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
    EXPECT_EQ(2u, mockOsMemory->reservationSizes.size());
    EXPECT_EQ(reservedLowSize, mockOsMemory->reservationSizes[0]);
    EXPECT_EQ(reservedLowSize2, mockOsMemory->reservationSizes[1]);
    EXPECT_EQ(reinterpret_cast<void *>(0x10000), mockOsMemory->validReturnAddress);
    EXPECT_TRUE(mockOsMemory->getMemoryMapsCalled);
    verifyReservedHeaps(0x10000, 0x10000 + reservedLowSize2, gpuAddressSpace);

    // No space in high 48 bit, no space in low 48 bit, bail out nicely
    resetGfxPartition();

    mockOsMemory->forceParseMemoryMaps = true;
    mockOsMemory->memoryMaps = {{0x7ffff7ff3000ull, 0x7ffff7ffb000ull}, {0x7ffff7ffc000ull, 0x7ffff7ffd000ull}, {0x7ffff7ffd000ull, 0x7ffff7ffe000ull}, {0x7ffff7ffe000ull, 0x7ffff7fff000ull}, {0x800000000000ull, 0x1000000000000ull}, {0xffffffffff600000ull, 0xffffffffff601000ull}};

    mockOsMemory->invalidReturnAddress = nullptr;
    mockOsMemory->returnInvalidAddressAlways = true;

    EXPECT_FALSE(gfxPartition->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));
}

TEST_F(GfxPartitionOn57bTest, whenInitGfxPartitionThenDoubleHeapStandardAtTheCostOfStandard64AndStandard2MB) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto gpuAddressSpace = 57;
    uint64_t gfxTop = maxNBitValue(gpuAddressSpace) + 1;
    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    std::vector<std::unique_ptr<MockGfxPartition>> gfxPartitions;
    gfxPartitions.push_back(std::make_unique<MockGfxPartition>(reservedCpuAddressRange));
    gfxPartitions[0]->osMemory.reset(new MockOsMemory);
    EXPECT_TRUE(gfxPartitions[0]->init(maxNBitValue(gpuAddressSpace), 0, 0, 1, false, 0u, gfxTop));

    EXPECT_EQ(gfxPartitions[0]->getHeapSize(HeapIndex::heapStandard), 4 * gfxPartitions[0]->getHeapSize(HeapIndex::heapStandard64KB));
    EXPECT_EQ(gfxPartitions[0]->getHeapSize(HeapIndex::heapStandard), 4 * gfxPartitions[0]->getHeapSize(HeapIndex::heapStandard2MB));
}

TEST_F(GfxPartitionOn57bTest, given48bitGpuAddressSpaceAnd57bitCpuAddressWidthWhenInitializingMultipleGfxPartitionsThenReserveSpaceForSvmHeapOnlyOnce) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto gpuAddressSpace = 48;

    // 57 bit CPU VA, la57 is present - reserve high or low CPU address range depending of memory maps
    CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(57, "la57");
    uint64_t gfxTop = maxNBitValue(gpuAddressSpace) + 1;

    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    std::vector<std::unique_ptr<MockGfxPartition>> gfxPartitions;
    for (int i = 0; i < 10; ++i) {
        gfxPartitions.push_back(std::make_unique<MockGfxPartition>(reservedCpuAddressRange));
        gfxPartitions[i]->osMemory.reset(new MockOsMemory);
        EXPECT_TRUE(gfxPartitions[i]->init(maxNBitValue(gpuAddressSpace), 0, i, 10, false, 0u, gfxTop));
    }

    EXPECT_EQ(1u, static_cast<MockOsMemory *>(gfxPartitions[0]->osMemory.get())->getReserveCount());
}

TEST_F(GfxPartitionOn57bTest, given57bitGpuAddressSpaceAnd57bitCpuAddressWidthWhenInitializingMultipleGfxPartitionsThenReserveSpaceForSvmHeapAndExtendedHeapsPerGfxPartition) {
    if (is32bit) {
        GTEST_SKIP();
    }

    auto gpuAddressSpace = 57;

    // 57 bit CPU VA, la57 is present - reserve high or low CPU address range depending of memory maps
    CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(57, "la57");

    uint64_t gfxTop = maxNBitValue(gpuAddressSpace) + 1;
    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    std::vector<std::unique_ptr<MockGfxPartition>> gfxPartitions;
    for (int i = 0; i < 10; ++i) {
        gfxPartitions.push_back(std::make_unique<MockGfxPartition>(reservedCpuAddressRange));
        gfxPartitions[i]->osMemory.reset(new MockOsMemory);
        EXPECT_TRUE(gfxPartitions[i]->init(maxNBitValue(gpuAddressSpace), 0, i, 10, false, 0u, gfxTop));
    }

    EXPECT_EQ(11u, static_cast<MockOsMemory *>(gfxPartitions[0]->osMemory.get())->getReserveCount());
}

TEST(GfxPartitionTest, givenGpuAddressSpaceIs57BitAndSeveralRootDevicesThenHeapExtendedIsSplitted) {
    if (is32bit) {
        GTEST_SKIP();
    }

    uint32_t rootDeviceIndex = 3;
    size_t numRootDevices = 5;

    uint64_t gfxTop = maxNBitValue(57) + 1;
    {
        // 57 bit CPU VA, la57 flag is present
        CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(57, "la57");
        VariableBackup<bool> backupAllowExtendedPointers(&SysCalls::mmapAllowExtendedPointers, true);

        MockGfxPartition gfxPartition;
        auto systemMemorySize = MemoryConstants::teraByte;
        EXPECT_TRUE(gfxPartition.init(maxNBitValue(57), reservedCpuAddressRangeSize, rootDeviceIndex, numRootDevices, false, systemMemorySize, gfxTop));

        auto heapExtendedSize = 4 * systemMemorySize;

        EXPECT_EQ(heapExtendedSize, gfxPartition.getHeapSize(HeapIndex::heapExtendedHost));
        EXPECT_LT(maxNBitValue(48), gfxPartition.getHeapBase(HeapIndex::heapExtendedHost));
    }

    {
        // 57 bit CPU VA, la57 flag is not present
        CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(57);

        MockGfxPartition gfxPartition;
        EXPECT_TRUE(gfxPartition.init(maxNBitValue(57), reservedCpuAddressRangeSize, rootDeviceIndex, numRootDevices, false, 0u, gfxTop));

        auto heapExtendedTotalSize = maxNBitValue(48) + 1;
        auto heapExtendedSize = alignDown(heapExtendedTotalSize, GfxPartition::heapGranularity);

        EXPECT_EQ(heapExtendedSize, gfxPartition.getHeapSize(HeapIndex::heapExtended));
        EXPECT_EQ(maxNBitValue(56) + 1 + rootDeviceIndex * heapExtendedSize, gfxPartition.getHeapBase(HeapIndex::heapExtended));
    }

    {
        // 48 bit CPU VA
        CpuInfoOverrideVirtualAddressSizeAndFlags overrideCpuInfo(48);

        MockGfxPartition gfxPartition;
        EXPECT_TRUE(gfxPartition.init(maxNBitValue(57), reservedCpuAddressRangeSize, rootDeviceIndex, numRootDevices, false, 0u, gfxTop));

        auto heapExtendedTotalSize = maxNBitValue(48) + 1;
        auto heapExtendedSize = alignDown(heapExtendedTotalSize, GfxPartition::heapGranularity);

        EXPECT_EQ(heapExtendedSize, gfxPartition.getHeapSize(HeapIndex::heapExtended));
        EXPECT_EQ(maxNBitValue(56) + 1 + rootDeviceIndex * heapExtendedSize, gfxPartition.getHeapBase(HeapIndex::heapExtended));
    }
}

TEST(GfxPartitionTest, givenHeapIndexWhenCheckingIsAnyHeap32ThenTrueIsReturnedFor32BitHeapsOnly) {

    HeapIndex heaps32[] = {HeapIndex::heapInternalDeviceMemory,
                           HeapIndex::heapInternal,
                           HeapIndex::heapExternalDeviceMemory,
                           HeapIndex::heapExternal,
                           HeapIndex::heapExternalFrontWindow,
                           HeapIndex::heapExternalDeviceFrontWindow,
                           HeapIndex::heapInternalFrontWindow,
                           HeapIndex::heapInternalDeviceFrontWindow};

    for (size_t i = 0; i < sizeof(heaps32) / sizeof(heaps32[0]); i++) {
        EXPECT_TRUE(GfxPartition::isAnyHeap32(heaps32[i]));
    }

    HeapIndex heapsOther[] = {
        HeapIndex::heapStandard,
        HeapIndex::heapStandard64KB,
        HeapIndex::heapStandard2MB,
        HeapIndex::heapSvm,
        HeapIndex::heapExtended,
        HeapIndex::heapExtendedHost};

    for (size_t i = 0; i < sizeof(heapsOther) / sizeof(heapsOther[0]); i++) {
        EXPECT_FALSE(GfxPartition::isAnyHeap32(heapsOther[i]));
    }
}
