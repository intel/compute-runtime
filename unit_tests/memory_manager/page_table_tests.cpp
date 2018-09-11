/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/selectors.h"
#include "runtime/memory_manager/page_table.h"
#include "runtime/memory_manager/page_table.inl"
#include "test.h"
#include "gtest/gtest.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_physical_address_allocator.h"

#include <memory>

using namespace OCLRT;

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

    uintptr_t map(uintptr_t vm, size_t size, uint32_t memoryBank) override {
        return PTE::map(vm, size, memoryBank);
    }
    void pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker, uint32_t memoryBank) override {
        return PTE::pageWalk(vm, size, offset, pageWalker, memoryBank);
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

class PPGTTPageTable : public TypeSelector<PML4, PDPE, sizeof(void *) == 8>::type {
  public:
    const size_t ppgttEntries = IntSelector<512u, 4u, sizeof(void *) == 8>::value;
    PPGTTPageTable(PhysicalAddressAllocator *allocator) : TypeSelector<PML4, PDPE, sizeof(void *) == 8>::type(allocator) {
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
    const uintptr_t refAddr = (uintptr_t(1) << IntSelector<46, 31, is64Bit>::value);
    MockPhysicalAddressAllocator allocator;
    uint64_t startAddress = 0x1000;

  public:
    void SetUp() {
        startAddress = 0x1000;
    }

    void TearDown() {
    }
};

typedef Test<PageTableFixture> PageTableTests32;

typedef Test<PageTableFixture> PageTableTests48;
typedef Test<PageTableFixture> PageTableTestsGPU;

TEST_F(PageTableTests48, dummy) {
    PageTable<void, 0, 9> pt(&allocator);

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset) {
    };

    pt.pageWalk(0, pageSize, 0, walker, PageTableHelper::memoryBankNotSpecified);
}

TEST_F(PageTableTests48, newIsEmpty) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    EXPECT_TRUE(pageTable->isEmpty());
    EXPECT_EQ(allocator.initialPageAddress, allocator.nextPageAddress);
}

TEST_F(PageTableTests48, DISABLED_mapSizeZero) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    EXPECT_TRUE(pageTable->isEmpty());
    EXPECT_EQ(allocator.initialPageAddress, allocator.nextPageAddress);

    auto phys1 = pageTable->map(0x0, 0x0, PageTableHelper::memoryBankNotSpecified);
    std::cerr << phys1 << std::endl;
}

TEST_F(PageTableTests48, pageWalkSimple) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr + (510 * pageSize) + 0x10;
    size_t lSize = 8 * pageSize;

    size_t walked = 0u;
    size_t lastOffset = 0;
    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset) {
        EXPECT_EQ(lastOffset, offset);
        EXPECT_GE(pageSize, size);

        walked += size;
        lastOffset += size;
    };
    pageTable->pageWalk(addr1, lSize, 0, walker, PageTableHelper::memoryBankNotSpecified);
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
        auto address = allocator.nextPageAddress.load();

        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset) {
            walked += size;
        };
        pageTable->pageWalk(gpuVa, size, 0, walker, PageTableHelper::memoryBankNotSpecified);

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

TEST_F(PageTableTests48, givenPageTableWhenMappingTheSameAddressMultipleTimesThenNumberOfPagesReservedInAllocatorMatchPagesMapped) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t address = refAddr;

    auto initialAddress = allocator.initialPageAddress;

    auto phys1 = pageTable->map(address, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1);

    auto phys1_1 = pageTable->map(address, 1, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1_1);

    auto phys2 = pageTable->map(address, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(phys1, phys2);

    address = ptrOffset(address, pageSize);
    auto phys3 = pageTable->map(address, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(phys1, phys3);

    address = ptrOffset(address, pageSize);
    auto phys4 = pageTable->map(address, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(phys3, phys4);

    auto nextFreeAddress = initialAddress + ptrDiff(phys4 + pageSize, initialAddress);

    EXPECT_EQ(nextFreeAddress, allocator.nextPageAddress.load());
}

TEST_F(PageTableTests48, physicalAddressesInAUBCantStartAt0) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(0u, phys1);
}

TEST_F(PageTableTests48, mapPageMapByteInMapped) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize, 0);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.nextPageAddress);

    auto phys1_1 = pageTable->map(addr1, 1, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1_1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.nextPageAddress);
}

TEST_F(PageTableTests48, mapsCorrectlyEvenMultipleCalls) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.nextPageAddress);

    auto phys1_1 = pageTable->map(addr1, 1, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1_1);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.nextPageAddress);

    auto phys2 = pageTable->map(addr1, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(phys1, phys2);
    EXPECT_EQ(allocator.initialPageAddress + pageSize, allocator.nextPageAddress);

    auto phys3 = pageTable->map(addr1 + pageSize, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(phys1, phys3);
    EXPECT_EQ(allocator.initialPageAddress + 2 * pageSize, allocator.nextPageAddress);

    auto phys4 = pageTable->map(addr1 + pageSize, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(phys1, phys3);
    EXPECT_EQ(phys3, phys4);

    auto addr2 = addr1 + pageSize + pageSize;
    auto phys5 = pageTable->map(addr2, 2 * pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(phys1, phys5);
    EXPECT_NE(phys3, phys5);

    auto phys6 = pageTable->map(addr2, 2 * pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_NE(phys1, phys6);
    EXPECT_NE(phys3, phys6);
    EXPECT_EQ(phys5, phys6);

    auto phys7 = pageTable->map(addr2 + pageSize, pageSize, PageTableHelper::memoryBankNotSpecified);
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

    auto phys1 = pageTable->map(addr1, size, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1);
}

TEST_F(PageTableTests48, mapsPagesOnTableBoundary2ndAllocation) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable(&allocator));
    uintptr_t addr1 = refAddr + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(0x0, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1);

    auto phys2 = pageTable->map(addr1, size, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress + pageSize, phys2);
}

TEST_F(PageTableTestsGPU, mapsPagesOnTableBoundary) {
    std::unique_ptr<GGTTPageTable> ggtt(new GGTTPageTable(&allocator));
    std::unique_ptr<PPGTTPageTable> ppgtt(new PPGTTPageTable(&allocator));
    uintptr_t addrGGTT = 0x70000000 + pageSize * 16;
    uintptr_t addrPPGTT = refAddr + pageSize * 16;

    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys32 = ggtt->map(addrGGTT, size, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys32);

    auto phys48 = ppgtt->map(addrPPGTT, size, PageTableHelper::memoryBankNotSpecified);
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
    auto phys = pt->map(0x10000, pageSize, PageTableHelper::memoryBankNotSpecified);
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

    auto phys1 = pageTable->map(addr1, size, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1);
}

TEST_F(PageTableTests32, mapsPagesOnTableBoundary2ndAllocation) {
    std::unique_ptr<GGTTPageTable> pageTable(new GGTTPageTable(&allocator));
    uintptr_t addr1 = 0x70000000 + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(0x0, pageSize, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress, phys1);

    auto phys2 = pageTable->map(addr1, size, PageTableHelper::memoryBankNotSpecified);
    EXPECT_EQ(startAddress + pageSize, phys2);
}
