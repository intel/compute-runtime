/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/page_table.h"
#include "runtime/memory_manager/page_table.inl"
#include "test.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_physical_address_allocator.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

static const bool is64Bit = (sizeof(void *) == 8);

template <class T, uint32_t level, uint32_t bits = 9>
class MockPageTable : public PageTable<T, level, bits> {
  public:
    using PageTable<T, level, bits>::PageTable;
    using PageTable<T, level, bits>::entries;
};

class MockPTE : public PTE {
  public:
    using PTE::entries;

    MockPTE(PhysicalAddressAllocator *physicalAddressAllocator) : PTE(physicalAddressAllocator) {}

    uintptr_t map(uintptr_t vm, size_t size, uint64_t entryBits, uint32_t memoryBank) override {
        return PTE::map(vm, size, entryBits, memoryBank);
    }
    void pageWalk(uintptr_t vm, size_t size, size_t offset, uint64_t entryBits, PageWalker &pageWalker, uint32_t memoryBank) override {
        return PTE::pageWalk(vm, size, offset, entryBits, pageWalker, memoryBank);
    }
};

class MockPDE : public MockPageTable<MockPTE, 1> {
  public:
    using MockPageTable<MockPTE, 1>::entries;
    MockPDE(PhysicalAddressAllocator *physicalAddressAllocator) : MockPageTable<MockPTE, 1>(physicalAddressAllocator) {
    }
};

class MockPDP : public MockPageTable<MockPDE, 2> {
  public:
    using MockPageTable<MockPDE, 2>::entries;
    MockPDP(PhysicalAddressAllocator *physicalAddressAllocator) : MockPageTable<MockPDE, 2>(physicalAddressAllocator) {
    }
};

class MockPML4 : public MockPageTable<MockPDP, 3> {
  public:
    using MockPageTable<MockPDP, 3>::entries;
    using PageTable<MockPDP, 3>::allocator;

    MockPML4(PhysicalAddressAllocator *physicalAddressAllocator) : MockPageTable<MockPDP, 3>(physicalAddressAllocator) {
    }
};

class MockPDPE : public MockPageTable<MockPDE, 2, 2> {
  public:
    using MockPageTable<MockPDE, 2, 2>::entries;
    using PageTable<MockPDE, 2, 2>::allocator;

    MockPDPE(PhysicalAddressAllocator *physicalAddressAllocator) : MockPageTable<MockPDE, 2, 2>(physicalAddressAllocator) {
    }
};

class PPGTTPageTable : public std::conditional<is64bit, PML4, PDPE>::type {
  public:
    const size_t ppgttEntries = is64bit ? 512u : 4u;
    PPGTTPageTable(PhysicalAddressAllocator *allocator) : std::conditional<is64bit, PML4, PDPE>::type(allocator) {
        EXPECT_EQ(ppgttEntries, entries.size());
    }
    bool isEmpty() {
        for (const auto &e : entries)
            if (e != nullptr)
                return false;
        return true;
    }
};

class GGTTPageTable : public PDPE {
  public:
    GGTTPageTable(PhysicalAddressAllocator *allocator) : PDPE(allocator) {
        EXPECT_EQ(4u, entries.size());
    }
    bool isEmpty() {
        for (const auto &e : entries)
            if (e != nullptr)
                return false;
        return true;
    }
};

class PageTableFixture {
  protected:
    const size_t pageSize = 1 << 12;
    const uintptr_t refAddr = uintptr_t(1) << (is64Bit ? 46 : 31);
    MockPhysicalAddressAllocator allocator;
    uint64_t startAddress = 0x1000;

  public:
    void SetUp() {
        startAddress = 0x1000;
    }

    void TearDown() {
    }
};

class PageTableEntryChecker {
  public:
    template <class T>
    static void testEntry(T *pageTable, uint32_t pteIndex, uintptr_t expectedValue) {
    }
};

template <>
void PageTableEntryChecker::testEntry<MockPML4>(MockPML4 *pageTable, uint32_t pteIndex, uintptr_t expectedValue) {
    ASSERT_NE(nullptr, pageTable->entries[0]);
    ASSERT_NE(nullptr, pageTable->entries[0]->entries[0]);
    ASSERT_NE(nullptr, pageTable->entries[0]->entries[0]->entries[0]);
    EXPECT_EQ(reinterpret_cast<void *>(expectedValue), pageTable->entries[0]->entries[0]->entries[0]->entries[pteIndex]);
}

template <>
void PageTableEntryChecker::testEntry<MockPDPE>(MockPDPE *pageTable, uint32_t pteIndex, uintptr_t expectedValue) {
    ASSERT_NE(nullptr, pageTable->entries[0]);
    EXPECT_EQ(reinterpret_cast<void *>(expectedValue), pageTable->entries[0]->entries[0]->entries[pteIndex]);
}

typedef Test<PageTableFixture> PageTableTests32;
typedef Test<PageTableFixture> PageTableTests48;
typedef Test<PageTableFixture> PageTableTestsGPU;

TEST_F(PageTableTests48, dummy) {
    PageTable<void, 0, 9> pt(&allocator);

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
    };

    pt.pageWalk(0, pageSize, 0, 0, walker, MemoryBanks::MainBank);
}

TEST_F(PageTableTests48, newIsEmpty) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    EXPECT_TRUE(pageTable->isEmpty());
}

TEST_F(PageTableTests48, DISABLED_mapSizeZero) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    EXPECT_TRUE(pageTable->isEmpty());

    auto phys1 = pageTable->map(0x0, 0x0, 0, MemoryBanks::MainBank);
    std::cerr << phys1 << std::endl;
}

TEST_F(PageTableTests48, pageWalkSimple) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr + (510 * pageSize) + 0x10;
    size_t lSize = 8 * pageSize;

    size_t walked = 0u;
    size_t lastOffset = 0;
    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        EXPECT_EQ(lastOffset, offset);
        EXPECT_GE(pageSize, size);

        walked += size;
        lastOffset += size;
    };
    pageTable->pageWalk(addr1, lSize, 0, 0, walker, MemoryBanks::MainBank);
    EXPECT_EQ(lSize, walked);
}

TEST_F(PageTableTests48, givenReservedPhysicalAddressWhenPageWalkIsCalledThenPageTablesAreFilledWithProperAddresses) {
    if (is64Bit) {
        std::unique_ptr<MockPML4> pageTable(std::make_unique<MockPML4>(&allocator));

        int shiftPML4 = is64Bit ? (9 + 9 + 9 + 12) : 0;
        int shiftPDP = is64Bit ? (9 + 9 + 12) : 0;

        uintptr_t gpuVa = (uintptr_t(0x1) << (shiftPML4)) | (uintptr_t(0x1) << (shiftPDP)) | (uintptr_t(0x1) << (9 + 12)) | 0x100;

        size_t size = 10 * pageSize;

        size_t walked = 0u;
        auto address = allocator.mainAllocator.load();

        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
            walked += size;
        };
        pageTable->pageWalk(gpuVa, size, 0, 0, walker, MemoryBanks::MainBank);

        EXPECT_EQ(size, walked);

        ASSERT_NE(nullptr, pageTable->entries[1]);
        ASSERT_NE(nullptr, pageTable->entries[1]->entries[1]);
        ASSERT_NE(nullptr, pageTable->entries[1]->entries[1]->entries[1]);

        for (uint32_t i = 0; i < 10; i++) {
            EXPECT_EQ(reinterpret_cast<void *>(address | 0x1), pageTable->entries[1]->entries[1]->entries[1]->entries[i]);
            address += pageSize;
        }
    }
}

TEST_F(PageTableTests48, givenBigGpuAddressWhenPageWalkIsCalledThenPageTablesAreFilledWithProperAddresses) {
    if (is64Bit) {
        std::unique_ptr<MockPML4> pageTable(std::make_unique<MockPML4>(&allocator));

        int shiftPML4 = is64Bit ? (47) : 0;
        int shiftPDP = is64Bit ? (9 + 9 + 12) : 0;

        uintptr_t gpuVa = (uintptr_t(0x1) << (shiftPML4)) | (uintptr_t(0x1) << (shiftPDP)) | (uintptr_t(0x1) << (9 + 12)) | 0x100;

        size_t size = 10 * pageSize;

        size_t walked = 0u;
        auto address = allocator.mainAllocator.load();

        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
            walked += size;
        };
        pageTable->pageWalk(gpuVa, size, 0, 0, walker, MemoryBanks::MainBank);

        EXPECT_EQ(size, walked);

        ASSERT_NE(nullptr, pageTable->entries[0x100]);
        ASSERT_NE(nullptr, pageTable->entries[0x100]->entries[1]);
        ASSERT_NE(nullptr, pageTable->entries[0x100]->entries[1]->entries[1]);

        for (uint32_t i = 0; i < 10; i++) {
            EXPECT_EQ(reinterpret_cast<void *>(address | 0x1), pageTable->entries[0x100]->entries[1]->entries[1]->entries[i]);
            address += pageSize;
        }
    }
}

TEST_F(PageTableTests48, givenZeroEntryBitsWhenPageWalkIsCalledThenPageTableEntryHasPresentBitSet) {
    std::unique_ptr<std::conditional<is64bit, MockPML4, MockPDPE>::type>
        pageTable(std::make_unique<std::conditional<is64bit, MockPML4, MockPDPE>::type>(&allocator));

    uintptr_t gpuVa = 0x1000;

    size_t size = pageSize;

    size_t walked = 0u;
    auto address = allocator.mainAllocator.load();

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        walked += size;
    };
    pageTable->pageWalk(gpuVa, size, 0, 0, walker, MemoryBanks::MainBank);

    EXPECT_EQ(size, walked);
    ASSERT_NE(nullptr, pageTable->entries[0]);

    PageTableEntryChecker::testEntry<std::conditional<is64bit, MockPML4, MockPDPE>::type>(pageTable.get(), 1, static_cast<uintptr_t>(address | 0x1));
}

TEST_F(PageTableTests48, givenZeroEntryBitsWhenMapIsCalledThenPageTableEntryHasPresentBitSet) {
    std::unique_ptr<std::conditional<is64bit, MockPML4, MockPDPE>::type>
        pageTable(std::make_unique<std::conditional<is64bit, MockPML4, MockPDPE>::type>(&allocator));
    uintptr_t gpuVa = 0x1000;
    size_t size = pageSize;
    auto address = allocator.mainAllocator.load();

    pageTable->map(gpuVa, size, 0, MemoryBanks::MainBank);
    ASSERT_NE(nullptr, pageTable->entries[0]);

    PageTableEntryChecker::testEntry<std::conditional<is64bit, MockPML4, MockPDPE>::type>(pageTable.get(), 1, static_cast<uintptr_t>(address | 0x1));
}

TEST_F(PageTableTests48, givenEntryBitsWhenPageWalkIsCalledThenEntryBitsArePassedToPageWalker) {
    std::unique_ptr<std::conditional<is64bit, MockPML4, MockPDPE>::type>
        pageTable(std::make_unique<std::conditional<is64bit, MockPML4, MockPDPE>::type>(&allocator));

    uintptr_t gpuVa = 0x1000;
    size_t size = pageSize;
    size_t walked = 0u;
    uint64_t ppgttBits = 0xabc;
    uint64_t entryBitsPassed = 0u;

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        walked += size;
        entryBitsPassed = entryBits;
    };
    pageTable->pageWalk(gpuVa, size, 0, ppgttBits, walker, MemoryBanks::MainBank);

    ppgttBits |= 0x1;
    EXPECT_EQ(ppgttBits, entryBitsPassed);
}

TEST_F(PageTableTests48, givenTwoPageWalksWhenSecondWalkHasDifferentEntryBitsThenEntryIsUpdated) {
    std::unique_ptr<std::conditional<is64bit, MockPML4, MockPDPE>::type>
        pageTable(std::make_unique<std::conditional<is64bit, MockPML4, MockPDPE>::type>(&allocator));

    uintptr_t gpuVa = 0x1000;
    size_t size = pageSize;
    size_t walked = 0u;
    uint64_t ppgttBits = 0xabc;
    uint64_t entryBitsPassed = 0u;

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        walked += size;
        entryBitsPassed = entryBits;
    };
    pageTable->pageWalk(gpuVa, size, 0, ppgttBits, walker, MemoryBanks::MainBank);

    ppgttBits |= 0x1;
    EXPECT_EQ(ppgttBits, entryBitsPassed);

    ppgttBits = 0x345;
    pageTable->pageWalk(gpuVa, size, 0, ppgttBits, walker, MemoryBanks::MainBank);

    EXPECT_EQ(ppgttBits, entryBitsPassed);
}

TEST_F(PageTableTests48, givenTwoPageWalksWhenSecondWalkHasNonValidEntryBitsThenEntryIsNotUpdated) {
    std::unique_ptr<std::conditional<is64bit, MockPML4, MockPDPE>::type>
        pageTable(std::make_unique<std::conditional<is64bit, MockPML4, MockPDPE>::type>(&allocator));

    uintptr_t gpuVa = 0x1000;
    size_t size = pageSize;
    size_t walked = 0u;
    uint64_t ppgttBits = 0xabc;
    uint64_t entryBitsPassed = 0u;
    uint64_t entryBitsPassedFirstTime = 0u;
    uint64_t entryBitsPassedSecondTime = 0u;

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        walked += size;
        entryBitsPassed = entryBits;
    };
    pageTable->pageWalk(gpuVa, size, 0, ppgttBits, walker, 0);

    ppgttBits |= 0x1;
    EXPECT_EQ(ppgttBits, entryBitsPassed);
    entryBitsPassedFirstTime = entryBitsPassed;

    ppgttBits = PageTableEntry::nonValidBits;
    pageTable->pageWalk(gpuVa, size, 0, ppgttBits, walker, 0);

    entryBitsPassedSecondTime = entryBitsPassed;
    EXPECT_EQ(entryBitsPassedFirstTime, entryBitsPassedSecondTime);
}

TEST_F(PageTableTests48, givenTwoMapsWhenSecondMapHasDifferentEntryBitsThenEntryIsUpdated) {
    std::unique_ptr<std::conditional<is64bit, MockPML4, MockPDPE>::type>
        pageTable(std::make_unique<std::conditional<is64bit, MockPML4, MockPDPE>::type>(&allocator));
    uintptr_t gpuVa = 0x1000;
    size_t size = pageSize;
    uint64_t ppgttBits = 0xabc;
    auto address = allocator.mainAllocator.load();

    pageTable->map(gpuVa, size, ppgttBits, 0);
    ASSERT_NE(nullptr, pageTable->entries[0]);
    PageTableEntryChecker::testEntry<std::conditional<is64bit, MockPML4, MockPDPE>::type>(pageTable.get(), 1, static_cast<uintptr_t>(address | ppgttBits | 0x1));

    ppgttBits = 0x345;
    pageTable->map(gpuVa, size, ppgttBits, 0);
    PageTableEntryChecker::testEntry<std::conditional<is64bit, MockPML4, MockPDPE>::type>(pageTable.get(), 1, static_cast<uintptr_t>(address | ppgttBits | 0x1));
}

TEST_F(PageTableTests48, givenTwoMapsWhenSecondMapHasNonValidEntryBitsThenEntryIsNotUpdated) {
    std::unique_ptr<std::conditional<is64bit, MockPML4, MockPDPE>::type>
        pageTable(std::make_unique<std::conditional<is64bit, MockPML4, MockPDPE>::type>(&allocator));
    uintptr_t gpuVa = 0x1000;
    size_t size = pageSize;
    uint64_t ppgttBits = 0xabc;
    auto address = allocator.mainAllocator.load();

    pageTable->map(gpuVa, size, ppgttBits, 0);
    ASSERT_NE(nullptr, pageTable->entries[0]);

    PageTableEntryChecker::testEntry<std::conditional<is64bit, MockPML4, MockPDPE>::type>(pageTable.get(), 1, static_cast<uintptr_t>(address | ppgttBits | 0x1));

    uint64_t nonValidPpgttBits = PageTableEntry::nonValidBits;
    pageTable->map(gpuVa, size, nonValidPpgttBits, 0);

    PageTableEntryChecker::testEntry<std::conditional<is64bit, MockPML4, MockPDPE>::type>(pageTable.get(), 1, static_cast<uintptr_t>(address | ppgttBits | 0x1));
}

TEST_F(PageTableTests48, givenPageTableWhenMappingTheSameAddressMultipleTimesThenNumberOfPagesReservedInAllocatorMatchPagesMapped) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t address = refAddr;

    auto initialAddress = allocator.initialPageAddress;

    auto phys1 = pageTable->map(address, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1);

    auto phys1_1 = pageTable->map(address, 1, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1_1);

    auto phys2 = pageTable->map(address, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(phys1, phys2);

    address = ptrOffset(address, pageSize);
    auto phys3 = pageTable->map(address, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(phys1, phys3);

    address = ptrOffset(address, pageSize);
    auto phys4 = pageTable->map(address, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(phys3, phys4);

    auto nextFreeAddress = initialAddress + ptrDiff(phys4 + pageSize, initialAddress);

    EXPECT_EQ(nextFreeAddress, allocator.mainAllocator.load());
}

TEST_F(PageTableTests48, physicalAddressesInAUBCantStartAt0) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, phys1);
}

TEST_F(PageTableTests48, mapPageMapByteInMapped) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.mainAllocator);

    auto phys1_1 = pageTable->map(addr1, 1, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1_1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.mainAllocator);
}

TEST_F(PageTableTests48, mapsCorrectlyEvenMultipleCalls) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.mainAllocator);

    auto phys1_1 = pageTable->map(addr1, 1, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1_1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.mainAllocator);

    auto phys2 = pageTable->map(addr1, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(phys1, phys2);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.mainAllocator);

    auto phys3 = pageTable->map(addr1 + pageSize, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(phys1, phys3);
    EXPECT_EQ(allocator.initialPageAddress + 2 * pageSize, allocator.mainAllocator);

    auto phys4 = pageTable->map(addr1 + pageSize, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(phys1, phys3);
    EXPECT_EQ(phys3, phys4);

    auto addr2 = addr1 + pageSize + pageSize;
    auto phys5 = pageTable->map(addr2, 2 * pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(phys1, phys5);
    EXPECT_NE(phys3, phys5);

    auto phys6 = pageTable->map(addr2, 2 * pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(phys1, phys6);
    EXPECT_NE(phys3, phys6);
    EXPECT_EQ(phys5, phys6);

    auto phys7 = pageTable->map(addr2 + pageSize, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_NE(0u, phys7);
    EXPECT_NE(phys1, phys7);
    EXPECT_NE(phys3, phys7);
    EXPECT_NE(phys5, phys7);
    EXPECT_NE(phys6, phys7);
    EXPECT_EQ(phys6 + pageSize, phys7);
}

TEST_F(PageTableTests48, mapsPagesOnTableBoundary) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(addr1, size, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1);
}

TEST_F(PageTableTests48, mapsPagesOnTableBoundary2ndAllocation) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(0x0, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1);

    auto phys2 = pageTable->map(addr1, size, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress + pageSize, phys2);
}

TEST_F(PageTableTestsGPU, mapsPagesOnTableBoundary) {
    std::unique_ptr<GGTTPageTable> ggtt(new GGTTPageTable(&allocator));
    std::unique_ptr<PPGTTPageTable> ppgtt(new PPGTTPageTable(&allocator));
    uintptr_t addrGGTT = 0x70000000 + pageSize * 16;
    uintptr_t addrPPGTT = refAddr + pageSize * 16;

    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys32 = ggtt->map(addrGGTT, size, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys32);

    auto phys48 = ppgtt->map(addrPPGTT, size, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress + size, phys48);
}

TEST_F(PageTableTestsGPU, newIsEmpty) {
    std::unique_ptr<GGTTPageTable> ggtt(new GGTTPageTable(&allocator));
    EXPECT_TRUE(ggtt->isEmpty());

    std::unique_ptr<PPGTTPageTable> ppgtt(new PPGTTPageTable(&allocator));
    EXPECT_TRUE(ppgtt->isEmpty());
}

TEST_F(PageTableTests32, level0) {
    std::unique_ptr<PageTable<void, 0, 9>> pt(new PageTable<void, 0, 9>(&allocator));
    auto phys = pt->map(0x10000, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(0u, phys);
}

TEST_F(PageTableTests32, newIsEmpty) {
    std::unique_ptr<GGTTPageTable> pageTable(new GGTTPageTable(&allocator));
    EXPECT_TRUE(pageTable->isEmpty());
}

TEST_F(PageTableTests32, mapsPagesOnTableBoundary) {
    std::unique_ptr<GGTTPageTable> pageTable(new GGTTPageTable(&allocator));
    uintptr_t addr1 = 0x70000000 + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(addr1, size, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1);
}

TEST_F(PageTableTests32, mapsPagesOnTableBoundary2ndAllocation) {
    std::unique_ptr<GGTTPageTable> pageTable(new GGTTPageTable(&allocator));
    uintptr_t addr1 = 0x70000000 + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(0x0, pageSize, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress, phys1);

    auto phys2 = pageTable->map(addr1, size, 0, MemoryBanks::MainBank);
    EXPECT_EQ(startAddress + pageSize, phys2);
}
