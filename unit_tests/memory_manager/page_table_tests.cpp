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

#include "config.h"
#include "runtime/helpers/selectors.h"
#include "runtime/memory_manager/page_table.h"
#include "test.h"
#include "gtest/gtest.h"
#include "unit_tests/helpers/memory_management.h"

#include <memory>

using namespace OCLRT;

static const bool is64Bit = (sizeof(void *) == 8);

class PTEFixture : public PTE {
  public:
    const size_t pageSize = 1 << 12;

    void SetUp() {
        PTE::nextPage.store(PTE::initialPage);
        startAddress = PTE::initialPage * pageSize;
    }

    void TearDown() {
        PTE::nextPage.store(PTE::initialPage);
    }

    uint32_t getNextPage() {
        return nextPage.load();
    }

    uint64_t startAddress;
};

typedef Test<PTEFixture> PTETest;
TEST_F(PTETest, physicalAddressesInAUBCantStartAt0) {
    auto physAddress = nextPage.load();
    EXPECT_NE(0u, physAddress);
}

class PPGTTPageTable : public TypeSelector<PML4, PDPE, sizeof(void *) == 8>::type {
  public:
    const size_t ppgttEntries = IntSelector<512u, 4u, sizeof(void *) == 8>::value;
    PPGTTPageTable() {
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
    GGTTPageTable() {
        EXPECT_EQ(4u, entries.size());
    }
    bool isEmpty() {
        for (const auto &e : entries)
            if (e != nullptr)
                return false;
        return true;
    }
};

class PageTableFixture : public PTEFixture {
  protected:
    const uintptr_t refAddr = (uintptr_t(1) << IntSelector<46, 31, is64Bit>::value);

  public:
    void SetUp() {
        PTEFixture::SetUp();
    }

    void TearDown() {
        PTEFixture::TearDown();
    }
};

typedef Test<PageTableFixture> PageTableTests32;

typedef Test<PageTableFixture> PageTableTests48;
typedef Test<PageTableFixture> PageTableTestsGPU;

TEST_F(PageTableTests48, dummy) {
    PageTable<void, 0, 9> pt;

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset) {
    };

    pt.pageWalk(0, pageSize, 0, walker);
}

TEST_F(PageTableTests48, newIsEmpty) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable);
    EXPECT_TRUE(pageTable->isEmpty());
    EXPECT_EQ(PTE::initialPage, this->getNextPage());
}

TEST_F(PageTableTests48, DISABLED_mapSizeZero) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable);
    EXPECT_TRUE(pageTable->isEmpty());
    EXPECT_EQ(PTE::initialPage, this->getNextPage());

    auto phys1 = pageTable->map(0x0, 0x0);
    std::cerr << phys1 << std::endl;
}

TEST_F(PageTableTests48, pageWalkSimple) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable);
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
    pageTable->pageWalk(addr1, lSize, 0, walker);
    EXPECT_EQ(lSize, walked);
}

TEST_F(PageTableTests48, mapPageMapByteInMapped) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable);
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(PTE::initialPage + 1u, this->getNextPage());

    auto phys1_1 = pageTable->map(addr1, 1);
    EXPECT_EQ(startAddress, phys1_1);
    EXPECT_EQ(PTE::initialPage + 1u, this->getNextPage());
}

TEST_F(PageTableTests48, mapsCorrectlyEvenMultipleCalls) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable);
    uintptr_t addr1 = refAddr;

    auto phys1 = pageTable->map(addr1, pageSize);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(PTE::initialPage + 1u, this->getNextPage());

    auto phys1_1 = pageTable->map(addr1, 1);
    EXPECT_EQ(startAddress, phys1_1);
    EXPECT_EQ(PTE::initialPage + 1u, this->getNextPage());

    auto phys2 = pageTable->map(addr1, pageSize);
    EXPECT_EQ(phys1, phys2);
    EXPECT_EQ(PTE::initialPage + 1u, this->getNextPage());

    auto phys3 = pageTable->map(addr1 + pageSize, pageSize);
    EXPECT_NE(phys1, phys3);
    EXPECT_EQ(PTE::initialPage + 2u, this->getNextPage());

    auto phys4 = pageTable->map(addr1 + pageSize, pageSize);
    EXPECT_NE(phys1, phys3);
    EXPECT_EQ(phys3, phys4);
    EXPECT_EQ(PTE::initialPage + 2u, this->getNextPage());

    auto addr2 = addr1 + pageSize + pageSize;
    auto phys5 = pageTable->map(addr2, 2 * pageSize);
    EXPECT_NE(phys1, phys5);
    EXPECT_NE(phys3, phys5);
    EXPECT_EQ(PTE::initialPage + 4u, this->getNextPage());

    auto phys6 = pageTable->map(addr2, 2 * pageSize);
    EXPECT_NE(phys1, phys6);
    EXPECT_NE(phys3, phys6);
    EXPECT_EQ(phys5, phys6);
    EXPECT_EQ(PTE::initialPage + 4u, this->getNextPage());

    auto phys7 = pageTable->map(addr2 + pageSize, pageSize);
    EXPECT_NE(0u, phys7);
    EXPECT_NE(phys1, phys7);
    EXPECT_NE(phys3, phys7);
    EXPECT_NE(phys5, phys7);
    EXPECT_NE(phys6, phys7);
    EXPECT_EQ(phys6 + pageSize, phys7);
    EXPECT_EQ(PTE::initialPage + 4u, this->getNextPage());
}

TEST_F(PageTableTests48, mapsPagesOnTableBoundary) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable);
    uintptr_t addr1 = refAddr + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(addr1, size);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(PTE::initialPage + 1024u, this->getNextPage());
}

TEST_F(PageTableTests48, mapsPagesOnTableBoundary2ndAllocation) {
    std::unique_ptr<PPGTTPageTable> pageTable(new PPGTTPageTable);
    uintptr_t addr1 = refAddr + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(0x0, pageSize);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(PTE::initialPage + 1u, this->getNextPage());

    auto phys2 = pageTable->map(addr1, size);
    EXPECT_EQ(startAddress + pageSize, phys2);
    EXPECT_EQ(PTE::initialPage + 1025u, this->getNextPage());
}

TEST_F(PageTableTestsGPU, mapsPagesOnTableBoundary) {
    std::unique_ptr<GGTTPageTable> ggtt(new GGTTPageTable);
    std::unique_ptr<PPGTTPageTable> ppgtt(new PPGTTPageTable);
    uintptr_t addrGGTT = 0x70000000 + pageSize * 16;
    uintptr_t addrPPGTT = refAddr + pageSize * 16;

    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys32 = ggtt->map(addrGGTT, size);
    EXPECT_EQ(startAddress, phys32);
    EXPECT_EQ(PTE::initialPage + 1024u, this->getNextPage());

    auto phys48 = ppgtt->map(addrPPGTT, size);
    EXPECT_EQ(startAddress + size, phys48);
    EXPECT_EQ(PTE::initialPage + 2048u, this->getNextPage());
}

TEST_F(PageTableTestsGPU, newIsEmpty) {
    std::unique_ptr<GGTTPageTable> ggtt(new GGTTPageTable);
    EXPECT_TRUE(ggtt->isEmpty());
    EXPECT_EQ(PTE::initialPage, this->getNextPage());
    std::unique_ptr<PPGTTPageTable> ppgtt(new PPGTTPageTable);
    EXPECT_TRUE(ppgtt->isEmpty());
    EXPECT_EQ(PTE::initialPage, this->getNextPage());
}

TEST_F(PageTableTests32, level0) {
    std::unique_ptr<PageTable<void, 0, 9>> pt(new PageTable<void, 0, 9>());
    auto phys = pt->map(0x10000, pageSize);
    EXPECT_EQ(0u, phys);
}

TEST_F(PageTableTests32, newIsEmpty) {
    std::unique_ptr<GGTTPageTable> pageTable(new GGTTPageTable);
    EXPECT_TRUE(pageTable->isEmpty());
    EXPECT_EQ(PTE::initialPage, this->getNextPage());
}

TEST_F(PageTableTests32, mapsPagesOnTableBoundary) {
    std::unique_ptr<GGTTPageTable> pageTable(new GGTTPageTable);
    uintptr_t addr1 = 0x70000000 + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(addr1, size);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(PTE::initialPage + 1024u, this->getNextPage());
}

TEST_F(PageTableTests32, mapsPagesOnTableBoundary2ndAllocation) {
    std::unique_ptr<GGTTPageTable> pageTable(new GGTTPageTable);
    uintptr_t addr1 = 0x70000000 + pageSize * 16;
    size_t pages = (1 << 9) * 2;
    size_t size = pages * pageSize;

    auto phys1 = pageTable->map(0x0, pageSize);
    EXPECT_EQ(startAddress, phys1);
    EXPECT_EQ(PTE::initialPage + 1u, this->getNextPage());

    auto phys2 = pageTable->map(addr1, size);
    EXPECT_EQ(startAddress + pageSize, phys2);
    EXPECT_EQ(PTE::initialPage + 1025u, this->getNextPage());
}
