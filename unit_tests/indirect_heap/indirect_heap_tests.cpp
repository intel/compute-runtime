/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace NEO;

struct IndirectHeapTest : public ::testing::Test {
    uint8_t buffer[256];
    MockGraphicsAllocation gfxAllocation = {(void *)buffer, sizeof(buffer)};
    IndirectHeap indirectHeap = {&gfxAllocation};
};

TEST_F(IndirectHeapTest, getSpaceTestSizeZero) {
    EXPECT_NE(nullptr, indirectHeap.getSpace(0));
}

TEST_F(IndirectHeapTest, getSpaceTestSizeNonZero) {
    EXPECT_NE(nullptr, indirectHeap.getSpace(sizeof(uint32_t)));
}

TEST_F(IndirectHeapTest, getSpaceTestReturnsWritablePointer) {
    uint32_t cmd = 0xbaddf00d;
    auto pCmd = indirectHeap.getSpace(sizeof(cmd));
    ASSERT_NE(nullptr, pCmd);
    *(uint32_t *)pCmd = cmd;
}

TEST_F(IndirectHeapTest, getSpaceReturnsDifferentPointersForEachRequest) {
    auto pCmd = indirectHeap.getSpace(sizeof(uint32_t));
    ASSERT_NE(nullptr, pCmd);
    auto pCmd2 = indirectHeap.getSpace(sizeof(uint32_t));
    ASSERT_NE(pCmd2, pCmd);
}

TEST_F(IndirectHeapTest, getMaxAvailableSpace) {
    ASSERT_EQ(indirectHeap.getMaxAvailableSpace(), indirectHeap.getAvailableSpace());
}

TEST_F(IndirectHeapTest, getAvailableSpaceShouldBeNonZeroAfterInit) {
    EXPECT_NE(0u, indirectHeap.getAvailableSpace());
}

TEST_F(IndirectHeapTest, getSpaceReducesAvailableSpace) {
    auto originalAvailable = indirectHeap.getAvailableSpace();
    indirectHeap.getSpace(sizeof(uint32_t));

    EXPECT_LT(indirectHeap.getAvailableSpace(), originalAvailable);
}

TEST_F(IndirectHeapTest, align) {
    size_t alignment = 64 * sizeof(uint8_t);
    indirectHeap.align(alignment);

    size_t size = 1;
    auto ptr = indirectHeap.getSpace(size);
    EXPECT_NE(nullptr, ptr);

    uintptr_t address = (uintptr_t)ptr;
    EXPECT_EQ(0u, address % alignment);
}

TEST_F(IndirectHeapTest, alignShouldAlignUpward) {
    size_t size = 1;
    auto ptr1 = indirectHeap.getSpace(size);
    ASSERT_NE(nullptr, ptr1);

    size_t alignment = 64 * sizeof(uint8_t);
    indirectHeap.align(alignment);

    auto ptr2 = indirectHeap.getSpace(size);
    ASSERT_NE(nullptr, ptr2);

    // Should align up
    EXPECT_GT(ptr2, ptr1);
}

TEST_F(IndirectHeapTest, givenIndirectHeapWhenGetCpuBaseIsCalledThenCpuAddressIsReturned) {
    auto base = indirectHeap.getCpuBase();
    EXPECT_EQ(base, buffer);
}

TEST(IndirectHeapWith4GbAllocatorTest, givenIndirectHeapNotSupporting4GbModeWhenAskedForHeapGpuStartOffsetThenZeroIsReturned) {
    auto cpuBaseAddress = reinterpret_cast<void *>(0x3000);
    MockGraphicsAllocation graphicsAllocation(cpuBaseAddress, 4096u);
    graphicsAllocation.setGpuBaseAddress(4096u);
    IndirectHeap indirectHeap(&graphicsAllocation, false);

    EXPECT_EQ(0u, indirectHeap.getHeapGpuStartOffset());
}

TEST(IndirectHeapWith4GbAllocatorTest, givenIndirectHeapSupporting4GbModeWhenAskedForHeapGpuStartOffsetThenZeroIsReturned) {
    auto cpuBaseAddress = reinterpret_cast<void *>(0x3000);
    MockGraphicsAllocation graphicsAllocation(cpuBaseAddress, 4096u);
    graphicsAllocation.setGpuBaseAddress(4096u);
    IndirectHeap indirectHeap(&graphicsAllocation, true);

    EXPECT_EQ(8192u, indirectHeap.getHeapGpuStartOffset());
}

TEST(IndirectHeapWith4GbAllocatorTest, givenIndirectHeapSupporting4GbModeWhenAskedForHeapBaseThenGpuBaseIsReturned) {
    auto cpuBaseAddress = reinterpret_cast<void *>(0x2000);
    MockGraphicsAllocation graphicsAllocation(cpuBaseAddress, 4096u);
    graphicsAllocation.setGpuBaseAddress(4096u);
    IndirectHeap indirectHeap(&graphicsAllocation, true);

    EXPECT_EQ(4096u, indirectHeap.getHeapGpuBase());
}

TEST(IndirectHeapWith4GbAllocatorTest, givenIndirectHeapNotSupporting4GbModeWhenAskedForHeapBaseThenGpuAddressIsReturned) {
    auto cpuBaseAddress = reinterpret_cast<void *>(0x2000);
    MockGraphicsAllocation graphicsAllocation(cpuBaseAddress, 4096u);
    graphicsAllocation.setGpuBaseAddress(4096u);
    IndirectHeap indirectHeap(&graphicsAllocation, false);

    EXPECT_EQ(8192u, indirectHeap.getHeapGpuBase());
}

TEST(IndirectHeapWith4GbAllocatorTest, givenIndirectHeapNotSupporting4GbModeWhenAskedForHeapSizeThenGraphicsAllocationSizeInPagesIsReturned) {
    auto cpuBaseAddress = reinterpret_cast<void *>(0x2000);
    MockGraphicsAllocation graphicsAllocation(cpuBaseAddress, 4096u);
    graphicsAllocation.setGpuBaseAddress(4096u);
    IndirectHeap indirectHeap(&graphicsAllocation, false);

    EXPECT_EQ(1u, indirectHeap.getHeapSizeInPages());
}

TEST(IndirectHeapWith4GbAllocatorTest, givenIndirectHeapSupporting4GbModeWhenAskedForHeapSizeThen4GbSizeInPagesIsReturned) {
    auto cpuBaseAddress = reinterpret_cast<void *>(0x2000);
    MockGraphicsAllocation graphicsAllocation(cpuBaseAddress, 4096u);
    graphicsAllocation.setGpuBaseAddress(4096u);
    IndirectHeap indirectHeap(&graphicsAllocation, true);

    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, indirectHeap.getHeapSizeInPages());
}
