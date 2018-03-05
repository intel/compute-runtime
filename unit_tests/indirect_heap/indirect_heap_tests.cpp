/*
 * Copyright (c) 2017, Intel Corporation
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

#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace OCLRT;

struct IndirectHeapTest : public ::testing::Test {
    IndirectHeapTest() {
    }
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
